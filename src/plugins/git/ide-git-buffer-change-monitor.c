/* ide-git-buffer-change-monitor.c
 *
 * Copyright © 2015 Christian Hergert <christian@hergert.me>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "ide-git-buffer-change-monitor"

#include <dazzle.h>
#include <glib/gi18n.h>
#include <libgit2-glib/ggit.h>

#include "ide-git-buffer-change-monitor.h"
#include "ide-git-vcs.h"

/**
 * SECTION:idegitbufferchangemonitor
 *
 * This module provides line change monitoring when used in conjunction with an IdeGitVcs.
 * The changes are generated by comparing the buffer contents to the version found inside of
 * the git repository.
 *
 * To enable us to avoid blocking the main loop, the actual diff is performed in a background
 * thread. To avoid threading issues with the rest of LibIDE, this module creates a copy of the
 * loaded repository. A single thread will be dispatched for the context and all reload tasks
 * will be performed from that thread.
 *
 * Upon completion of the diff, the results will be passed back to the primary thread and the
 * state updated for use by line change renderer in the source view.
 *
 * TODO: Move the thread work into ide_thread_pool?
 */

struct _IdeGitBufferChangeMonitor
{
  IdeBufferChangeMonitor  parent_instance;

  DzlSignalGroup         *signal_group;
  DzlSignalGroup         *vcs_signal_group;

  IdeBuffer              *buffer;

  GgitRepository         *repository;
  GHashTable             *state;

  GgitBlob               *cached_blob;

  guint                   changed_timeout;

  guint                   state_dirty : 1;
  guint                   in_calculation : 1;
  guint                   delete_range_requires_recalculation : 1;
  guint                   is_child_of_workdir : 1;
};

typedef struct
{
  GgitRepository *repository;
  GHashTable     *state;
  GFile          *file;
  GBytes         *content;
  GgitBlob       *blob;
  guint           is_child_of_workdir : 1;
} DiffTask;

G_DEFINE_TYPE (IdeGitBufferChangeMonitor,
               ide_git_buffer_change_monitor,
               IDE_TYPE_BUFFER_CHANGE_MONITOR)

DZL_DEFINE_COUNTER (instances, "IdeGitBufferChangeMonitor", "Instances",
                    "The number of git buffer change monitor instances.");

enum {
  PROP_0,
  PROP_REPOSITORY,
  LAST_PROP
};

static GParamSpec  *properties [LAST_PROP];
static GAsyncQueue *work_queue;
static GThread     *work_thread;

static void
diff_task_free (gpointer data)
{
  DiffTask *diff = data;

  if (diff)
    {
      g_clear_object (&diff->file);
      g_clear_object (&diff->blob);
      g_clear_object (&diff->repository);
      g_clear_pointer (&diff->state, g_hash_table_unref);
      g_clear_pointer (&diff->content, g_bytes_unref);
    }
}

static GHashTable *
ide_git_buffer_change_monitor_calculate_finish (IdeGitBufferChangeMonitor  *self,
                                                GAsyncResult               *result,
                                                GError                    **error)
{
  GTask *task = (GTask *)result;
  DiffTask *diff;

  g_assert (IDE_IS_GIT_BUFFER_CHANGE_MONITOR (self));
  g_assert (G_IS_TASK (result));

  diff = g_task_get_task_data (task);

  /* Keep the blob around for future use */
  if (diff->blob != self->cached_blob)
    g_set_object (&self->cached_blob, diff->blob);

  /* If the file is a child of the working directory, we need to know */
  self->is_child_of_workdir = diff->is_child_of_workdir;

  return g_task_propagate_pointer (task, error);
}

static void
ide_git_buffer_change_monitor_calculate_async (IdeGitBufferChangeMonitor *self,
                                               GCancellable              *cancellable,
                                               GAsyncReadyCallback        callback,
                                               gpointer                   user_data)
{
  g_autoptr(GTask) task = NULL;
  DiffTask *diff;
  IdeFile *file;
  GFile *gfile;

  g_assert (IDE_IS_GIT_BUFFER_CHANGE_MONITOR (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (self->buffer != NULL);
  g_assert (self->repository != NULL);

  self->state_dirty = FALSE;

  task = g_task_new (self, cancellable, callback, user_data);

  file = ide_buffer_get_file (self->buffer);
  gfile = ide_file_get_file (file);

  if (!gfile)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_NOT_FOUND,
                               _("Cannot provide diff, no backing file provided."));
      return;
    }

  diff = g_slice_new0 (DiffTask);
  diff->file = g_object_ref (gfile);
  diff->repository = g_object_ref (self->repository);
  diff->state = g_hash_table_new (g_direct_hash, g_direct_equal);
  diff->content = ide_buffer_get_content (self->buffer);
  diff->blob = self->cached_blob ? g_object_ref (self->cached_blob) : NULL;

  g_task_set_task_data (task, diff, diff_task_free);

  self->in_calculation = TRUE;

  g_async_queue_push (work_queue, g_object_ref (task));
}

static IdeBufferLineChange
ide_git_buffer_change_monitor_get_change (IdeBufferChangeMonitor *monitor,
                                          guint                   line)
{
  IdeGitBufferChangeMonitor *self = (IdeGitBufferChangeMonitor *)monitor;
  gpointer key;
  gpointer value;

  if (self->state == NULL)
    {
      /*
       * If the file is within the working directory, synthesize line addition.
       */
      if (self->is_child_of_workdir)
        return IDE_BUFFER_LINE_CHANGE_ADDED;
      return IDE_BUFFER_LINE_CHANGE_NONE;
    }

  key = GINT_TO_POINTER (line + 1);
  value = g_hash_table_lookup (self->state, key);

  return GPOINTER_TO_INT (value);
}

static void
ide_git_buffer_change_monitor_set_repository (IdeGitBufferChangeMonitor *self,
                                              GgitRepository            *repository)
{
  g_return_if_fail (IDE_IS_GIT_BUFFER_CHANGE_MONITOR (self));
  g_return_if_fail (GGIT_IS_REPOSITORY (repository));

  g_set_object (&self->repository, repository);
}

static void
ide_git_buffer_change_monitor__calculate_cb (GObject      *object,
                                             GAsyncResult *result,
                                             gpointer      user_data_unused)
{
  IdeGitBufferChangeMonitor *self = (IdeGitBufferChangeMonitor *)object;
  g_autoptr(GHashTable) ret = NULL;
  g_autoptr(GError) error = NULL;

  g_assert (IDE_IS_GIT_BUFFER_CHANGE_MONITOR (self));

  self->in_calculation = FALSE;

  ret = ide_git_buffer_change_monitor_calculate_finish (self, result, &error);

  if (!ret)
    {
      if (!g_error_matches (error, GGIT_ERROR, GGIT_ERROR_NOTFOUND))
        g_message ("%s", error->message);
    }
  else
    {
      g_clear_pointer (&self->state, g_hash_table_unref);
      self->state = g_hash_table_ref (ret);
    }

  ide_buffer_change_monitor_emit_changed (IDE_BUFFER_CHANGE_MONITOR (self));

  /*
   * Recalculate the state if the buffer has changed since we submitted our request.
   */
  if (self->state_dirty)
    ide_git_buffer_change_monitor_calculate_async (self,
                                                   NULL,
                                                   ide_git_buffer_change_monitor__calculate_cb,
                                                   NULL);
}

static void
ide_git_buffer_change_monitor_recalculate (IdeGitBufferChangeMonitor *self)
{
  g_assert (IDE_IS_GIT_BUFFER_CHANGE_MONITOR (self));

  self->state_dirty = TRUE;

  if (self->in_calculation)
    return;

  ide_git_buffer_change_monitor_calculate_async (self,
                                                 NULL,
                                                 ide_git_buffer_change_monitor__calculate_cb,
                                                 NULL);
}

static void
ide_git_buffer_change_monitor__buffer_delete_range_after_cb (IdeGitBufferChangeMonitor *self,
                                                             GtkTextIter               *begin,
                                                             GtkTextIter               *end,
                                                             IdeBuffer                 *buffer)
{
  IDE_ENTRY;

  g_assert (IDE_IS_GIT_BUFFER_CHANGE_MONITOR (self));
  g_assert (begin);
  g_assert (end);
  g_assert (IDE_IS_BUFFER (buffer));

  if (self->delete_range_requires_recalculation)
    {
      self->delete_range_requires_recalculation = FALSE;
      ide_git_buffer_change_monitor_recalculate (self);
    }

  IDE_EXIT;
}

static void
ide_git_buffer_change_monitor__buffer_delete_range_cb (IdeGitBufferChangeMonitor *self,
                                                       GtkTextIter               *begin,
                                                       GtkTextIter               *end,
                                                       IdeBuffer                 *buffer)
{
  IdeBufferLineChange change;

  IDE_ENTRY;

  g_assert (IDE_IS_GIT_BUFFER_CHANGE_MONITOR (self));
  g_assert (begin != NULL);
  g_assert (end != NULL);
  g_assert (IDE_IS_BUFFER (buffer));

  /*
   * We need to recalculate the diff when text is deleted if:
   *
   * 1) The range includes a newline.
   * 2) The current line change is set to NONE.
   *
   * Technically we need to do it on every change to be more correct, but that wastes a lot of
   * power. So instead, we'll be a bit lazy about it here and pick up the other changes on a much
   * more conservative timeout, generated by ide_git_buffer_change_monitor__buffer_changed_cb().
   */

  if (gtk_text_iter_get_line (begin) != gtk_text_iter_get_line (end))
    IDE_GOTO (recalculate);

  change = ide_git_buffer_change_monitor_get_change (IDE_BUFFER_CHANGE_MONITOR (self),
                                                     gtk_text_iter_get_line (begin));
  if (change == IDE_BUFFER_LINE_CHANGE_NONE)
    IDE_GOTO (recalculate);

  IDE_EXIT;

recalculate:
  /*
   * We need to wait for the delete to occur, so mark it as necessary and let
   * ide_git_buffer_change_monitor__buffer_delete_range_after_cb perform the operation.
   */
  self->delete_range_requires_recalculation = TRUE;

  IDE_EXIT;
}

static void
ide_git_buffer_change_monitor__buffer_insert_text_after_cb (IdeGitBufferChangeMonitor *self,
                                                            GtkTextIter               *location,
                                                            gchar                     *text,
                                                            gint                       len,
                                                            IdeBuffer                 *buffer)
{
  IdeBufferLineChange change;

  IDE_ENTRY;

  g_assert (IDE_IS_GIT_BUFFER_CHANGE_MONITOR (self));
  g_assert (location);
  g_assert (text);
  g_assert (IDE_IS_BUFFER (buffer));

  /*
   * We need to recalculate the diff when text is inserted if:
   *
   * 1) A newline is included in the text.
   * 2) The line currently has flags of NONE.
   *
   * Technically we need to do it on every change to be more correct, but that wastes a lot of
   * power. So instead, we'll be a bit lazy about it here and pick up the other changes on a much
   * more conservative timeout, generated by ide_git_buffer_change_monitor__buffer_changed_cb().
   */

  if (NULL != memmem (text, len, "\n", 1))
    IDE_GOTO (recalculate);

  change = ide_git_buffer_change_monitor_get_change (IDE_BUFFER_CHANGE_MONITOR (self),
                                                     gtk_text_iter_get_line (location));
  if (change == IDE_BUFFER_LINE_CHANGE_NONE)
    IDE_GOTO (recalculate);

  IDE_EXIT;

recalculate:
  ide_git_buffer_change_monitor_recalculate (self);

  IDE_EXIT;
}

static gboolean
ide_git_buffer_change_monitor__changed_timeout_cb (gpointer user_data)
{
  IdeGitBufferChangeMonitor *self = user_data;

  IDE_ENTRY;

  ide_git_buffer_change_monitor_recalculate (self);
  self->changed_timeout = 0;

  IDE_RETURN (G_SOURCE_REMOVE);
}

static void
ide_git_buffer_change_monitor__buffer_changed_after_cb (IdeGitBufferChangeMonitor *self,
                                                        IdeBuffer                 *buffer)
{
  IDE_ENTRY;

  g_assert (IDE_IS_BUFFER_CHANGE_MONITOR (self));
  g_assert (IDE_IS_BUFFER (buffer));

  self->state_dirty = TRUE;

  if (self->in_calculation)
    IDE_EXIT;

  if (self->changed_timeout)
    g_source_remove (self->changed_timeout);

  self->changed_timeout = g_timeout_add_seconds (1,
                                                 ide_git_buffer_change_monitor__changed_timeout_cb,
                                                 self);

  IDE_EXIT;
}

static void
ide_git_buffer_change_monitor_reload (IdeBufferChangeMonitor *monitor)
{
  IdeGitBufferChangeMonitor *self = (IdeGitBufferChangeMonitor *)monitor;

  IDE_ENTRY;

  g_assert (IDE_IS_GIT_BUFFER_CHANGE_MONITOR (self));

  g_clear_object (&self->cached_blob);
  ide_git_buffer_change_monitor_recalculate (self);

  IDE_EXIT;
}

static void
ide_git_buffer_change_monitor__vcs_reloaded_cb (IdeGitBufferChangeMonitor *self,
                                                GgitRepository            *new_repository,
                                                IdeGitVcs                 *vcs)
{
  IDE_ENTRY;

  g_assert (IDE_IS_GIT_BUFFER_CHANGE_MONITOR (self));
  g_assert (IDE_IS_GIT_VCS (vcs));

  g_set_object (&self->repository, new_repository);

  ide_buffer_change_monitor_reload (IDE_BUFFER_CHANGE_MONITOR (self));

  IDE_EXIT;
}

static void
ide_git_buffer_change_monitor_set_buffer (IdeBufferChangeMonitor *monitor,
                                          IdeBuffer              *buffer)
{
  IdeGitBufferChangeMonitor *self = (IdeGitBufferChangeMonitor *)monitor;
  IdeContext *context;
  IdeVcs *vcs;

  IDE_ENTRY;

  g_return_if_fail (IDE_IS_GIT_BUFFER_CHANGE_MONITOR (self));
  g_return_if_fail (IDE_IS_BUFFER (buffer));
  g_return_if_fail (!self->buffer);

  ide_set_weak_pointer (&self->buffer, buffer);

  context = ide_object_get_context (IDE_OBJECT (self));
  vcs = ide_context_get_vcs (context);

  dzl_signal_group_set_target (self->signal_group, buffer);
  dzl_signal_group_set_target (self->vcs_signal_group, vcs);

  IDE_EXIT;
}

static gint
diff_line_cb (GgitDiffDelta *delta,
              GgitDiffHunk  *hunk,
              GgitDiffLine  *line,
              gpointer       user_data)
{
  GgitDiffLineType type;
  GHashTable *hash = user_data;
  gint new_lineno;
  gint old_lineno;
  gint adjust;

  g_return_val_if_fail (delta, GGIT_ERROR_GIT_ERROR);
  g_return_val_if_fail (hunk, GGIT_ERROR_GIT_ERROR);
  g_return_val_if_fail (line, GGIT_ERROR_GIT_ERROR);
  g_return_val_if_fail (hash, GGIT_ERROR_GIT_ERROR);

  type = ggit_diff_line_get_origin (line);

  if ((type != GGIT_DIFF_LINE_ADDITION) && (type != GGIT_DIFF_LINE_DELETION))
    return 0;

  new_lineno = ggit_diff_line_get_new_lineno (line);
  old_lineno = ggit_diff_line_get_old_lineno (line);

  switch (type)
    {
    case GGIT_DIFF_LINE_ADDITION:
      if (g_hash_table_lookup (hash, GINT_TO_POINTER (new_lineno)))
        g_hash_table_replace (hash,
                              GINT_TO_POINTER (new_lineno),
                              GINT_TO_POINTER (IDE_BUFFER_LINE_CHANGE_CHANGED));
      else
        g_hash_table_insert (hash,
                             GINT_TO_POINTER (new_lineno),
                             GINT_TO_POINTER (IDE_BUFFER_LINE_CHANGE_ADDED));
      break;

    case GGIT_DIFF_LINE_DELETION:
      adjust = (ggit_diff_hunk_get_new_start (hunk) - ggit_diff_hunk_get_old_start (hunk));
      old_lineno += adjust;
      if (g_hash_table_lookup (hash, GINT_TO_POINTER (old_lineno)))
        g_hash_table_replace (hash,
                              GINT_TO_POINTER (old_lineno),
                              GINT_TO_POINTER (IDE_BUFFER_LINE_CHANGE_CHANGED));
      else
        g_hash_table_insert (hash,
                             GINT_TO_POINTER (old_lineno),
                             GINT_TO_POINTER (IDE_BUFFER_LINE_CHANGE_DELETED));
      break;

    case GGIT_DIFF_LINE_CONTEXT:
    case GGIT_DIFF_LINE_CONTEXT_EOFNL:
    case GGIT_DIFF_LINE_ADD_EOFNL:
    case GGIT_DIFF_LINE_DEL_EOFNL:
    case GGIT_DIFF_LINE_FILE_HDR:
    case GGIT_DIFF_LINE_BINARY:
    default:
      break;

    case GGIT_DIFF_LINE_HUNK_HDR:
      g_assert_not_reached ();
    }

  return 0;
}

static gboolean
ide_git_buffer_change_monitor_calculate_threaded (IdeGitBufferChangeMonitor  *self,
                                                  DiffTask                   *diff,
                                                  GError                    **error)
{
  g_autofree gchar *relative_path = NULL;
  g_autoptr(GFile) workdir = NULL;
  const guint8 *data;
  gsize data_len = 0;

  g_assert (IDE_IS_GIT_BUFFER_CHANGE_MONITOR (self));
  g_assert (diff);
  g_assert (G_IS_FILE (diff->file));
  g_assert (diff->state);
  g_assert (GGIT_IS_REPOSITORY (diff->repository));
  g_assert (diff->content);
  g_assert (!diff->blob || GGIT_IS_BLOB (diff->blob));
  g_assert (error);
  g_assert (!*error);

  workdir = ggit_repository_get_workdir (diff->repository);

  if (!workdir)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_FILENAME,
                   _("Repository does not have a working directory."));
      return FALSE;
    }

  relative_path = g_file_get_relative_path (workdir, diff->file);

  if (!relative_path)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_FILENAME,
                   _("File is not under control of git working directory."));
      return FALSE;
    }

  diff->is_child_of_workdir = TRUE;

  /*
   * Find the blob if necessary. This will be cached by the main thread for us on the way out
   * of the async operation.
   */
  if (!diff->blob)
    {
      GgitOId *entry_oid = NULL;
      GgitOId *oid = NULL;
      GgitObject *blob = NULL;
      GgitObject *commit = NULL;
      GgitRef *head = NULL;
      GgitTree *tree = NULL;
      GgitTreeEntry *entry = NULL;

      head = ggit_repository_get_head (diff->repository, error);
      if (!head)
        goto cleanup;

      oid = ggit_ref_get_target (head);
      if (!oid)
        goto cleanup;

      commit = ggit_repository_lookup (diff->repository, oid, GGIT_TYPE_COMMIT, error);
      if (!commit)
        goto cleanup;

      tree = ggit_commit_get_tree (GGIT_COMMIT (commit));
      if (!tree)
        goto cleanup;

      entry = ggit_tree_get_by_path (tree, relative_path, error);
      if (!entry)
        goto cleanup;

      entry_oid = ggit_tree_entry_get_id (entry);
      if (!entry_oid)
        goto cleanup;

      blob = ggit_repository_lookup (diff->repository, entry_oid, GGIT_TYPE_BLOB, error);
      if (!blob)
        goto cleanup;

      diff->blob = g_object_ref (blob);

    cleanup:
      g_clear_object (&blob);
      g_clear_pointer (&entry_oid, ggit_oid_free);
      g_clear_pointer (&entry, ggit_tree_entry_unref);
      g_clear_object (&tree);
      g_clear_object (&commit);
      g_clear_pointer (&oid, ggit_oid_free);
      g_clear_object (&head);
    }

  if (!diff->blob)
    {
      if ((*error) == NULL)
        g_set_error (error,
                     G_IO_ERROR,
                     G_IO_ERROR_NOT_FOUND,
                     _("The requested file does not exist within the git index."));
      return FALSE;
    }

  data = g_bytes_get_data (diff->content, &data_len);

  ggit_diff_blob_to_buffer (diff->blob, relative_path, data, data_len, relative_path,
                            NULL, NULL, NULL, NULL, diff_line_cb, (gpointer)diff->state, error);

  return ((*error) == NULL);
}

static gpointer
ide_git_buffer_change_monitor_worker (gpointer data)
{
  GAsyncQueue *queue = data;
  GTask *task;

  g_assert (queue != NULL);

  /*
   * This is a single thread worker that dispatches the particular
   * change to the given change monitor. We require a single thread
   * so that we can mantain the invariant that only a single thread
   * can access a GgitRepository at a time (and change monitors all
   * share the same GgitRepository amongst themselves).
   */

  while ((task = g_async_queue_pop (queue)))
    {
      IdeGitBufferChangeMonitor *self;
      DiffTask *diff;
      GError *error = NULL;

      self = g_task_get_source_object (task);
      diff = g_task_get_task_data (task);

      if (!ide_git_buffer_change_monitor_calculate_threaded (self, diff, &error))
        g_task_return_error (task, error);
      else
        g_task_return_pointer (task, g_hash_table_ref (diff->state),
                               (GDestroyNotify)g_hash_table_unref);

      g_object_unref (task);
    }

  return NULL;
}

static void
ide_git_buffer_change_monitor_dispose (GObject *object)
{
  IdeGitBufferChangeMonitor *self = (IdeGitBufferChangeMonitor *)object;

  if (self->changed_timeout)
    {
      g_source_remove (self->changed_timeout);
      self->changed_timeout = 0;
    }

  ide_clear_weak_pointer (&self->buffer);

  g_clear_object (&self->signal_group);
  g_clear_object (&self->vcs_signal_group);
  g_clear_object (&self->cached_blob);
  g_clear_object (&self->repository);

  G_OBJECT_CLASS (ide_git_buffer_change_monitor_parent_class)->dispose (object);
}

static void
ide_git_buffer_change_monitor_finalize (GObject *object)
{
  G_OBJECT_CLASS (ide_git_buffer_change_monitor_parent_class)->finalize (object);

  DZL_COUNTER_DEC (instances);
}

static void
ide_git_buffer_change_monitor_set_property (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec)
{
  IdeGitBufferChangeMonitor *self = IDE_GIT_BUFFER_CHANGE_MONITOR (object);

  switch (prop_id)
    {
    case PROP_REPOSITORY:
      ide_git_buffer_change_monitor_set_repository (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_git_buffer_change_monitor_class_init (IdeGitBufferChangeMonitorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeBufferChangeMonitorClass *parent_class = IDE_BUFFER_CHANGE_MONITOR_CLASS (klass);

  object_class->dispose = ide_git_buffer_change_monitor_dispose;
  object_class->finalize = ide_git_buffer_change_monitor_finalize;
  object_class->set_property = ide_git_buffer_change_monitor_set_property;

  parent_class->set_buffer = ide_git_buffer_change_monitor_set_buffer;
  parent_class->get_change = ide_git_buffer_change_monitor_get_change;
  parent_class->reload = ide_git_buffer_change_monitor_reload;

  properties [PROP_REPOSITORY] =
    g_param_spec_object ("repository",
                         "Repository",
                         "The repository to use for calculating diffs.",
                         GGIT_TYPE_REPOSITORY,
                         (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  /* Note: We use a single worker thread so that we can maintain the
   *       invariant that only a single thread is touching the GgitRepository
   *       at a time. (Also, you can only type in one editor at a time, so
   *       on worker thread for interactive blob changes is fine.
   */
  work_queue = g_async_queue_new ();
  work_thread = g_thread_new ("IdeGitBufferChangeMonitorWorker",
                              ide_git_buffer_change_monitor_worker,
                              work_queue);
}

static void
ide_git_buffer_change_monitor_init (IdeGitBufferChangeMonitor *self)
{
  DZL_COUNTER_INC (instances);

  self->signal_group = dzl_signal_group_new (IDE_TYPE_BUFFER);
  dzl_signal_group_connect_object (self->signal_group,
                                   "insert-text",
                                   G_CALLBACK (ide_git_buffer_change_monitor__buffer_insert_text_after_cb),
                                   self,
                                   G_CONNECT_SWAPPED | G_CONNECT_AFTER);
  dzl_signal_group_connect_object (self->signal_group,
                                   "delete-range",
                                   G_CALLBACK (ide_git_buffer_change_monitor__buffer_delete_range_cb),
                                   self,
                                   G_CONNECT_SWAPPED);
  dzl_signal_group_connect_object (self->signal_group,
                                   "delete-range",
                                   G_CALLBACK (ide_git_buffer_change_monitor__buffer_delete_range_after_cb),
                                   self,
                                   G_CONNECT_SWAPPED | G_CONNECT_AFTER);
  dzl_signal_group_connect_object (self->signal_group,
                                   "changed",
                                   G_CALLBACK (ide_git_buffer_change_monitor__buffer_changed_after_cb),
                                   self,
                                   G_CONNECT_SWAPPED | G_CONNECT_AFTER);

  self->vcs_signal_group = dzl_signal_group_new (IDE_TYPE_GIT_VCS);
  dzl_signal_group_connect_object (self->vcs_signal_group,
                                   "reloaded",
                                   G_CALLBACK (ide_git_buffer_change_monitor__vcs_reloaded_cb),
                                   self,
                                   G_CONNECT_SWAPPED);
}