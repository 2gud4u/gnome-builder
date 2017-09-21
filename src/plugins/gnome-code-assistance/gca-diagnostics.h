/*
 * Generated by gdbus-codegen 2.42.0. DO NOT EDIT.
 *
 * The license of this code is the same as for the source it was derived from.
 */

#ifndef __GCA_DIAGNOSTICS_H__
#define __GCA_DIAGNOSTICS_H__

#include <gio/gio.h>

G_BEGIN_DECLS


/* ------------------------------------------------------------------------ */
/* Declarations for org.gnome.CodeAssist.v1.Diagnostics */

#define GCA_TYPE_DIAGNOSTICS (gca_diagnostics_get_type ())
#define GCA_DIAGNOSTICS(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), GCA_TYPE_DIAGNOSTICS, GcaDiagnostics))
#define GCA_IS_DIAGNOSTICS(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), GCA_TYPE_DIAGNOSTICS))
#define GCA_DIAGNOSTICS_GET_IFACE(o) (G_TYPE_INSTANCE_GET_INTERFACE ((o), GCA_TYPE_DIAGNOSTICS, GcaDiagnosticsIface))

struct _GcaDiagnostics;
typedef struct _GcaDiagnostics GcaDiagnostics;
typedef struct _GcaDiagnosticsIface GcaDiagnosticsIface;

struct _GcaDiagnosticsIface
{
  GTypeInterface parent_iface;

  gboolean (*handle_diagnostics) (
    GcaDiagnostics *object,
    GDBusMethodInvocation *invocation);

};

GType gca_diagnostics_get_type (void);

GDBusInterfaceInfo *gca_diagnostics_interface_info (void);
guint gca_diagnostics_override_properties (GObjectClass *klass, guint property_id_begin);


/* D-Bus method call completion functions: */
void gca_diagnostics_complete_diagnostics (
    GcaDiagnostics *object,
    GDBusMethodInvocation *invocation,
    GVariant *unnamed_arg0);



/* D-Bus method calls: */
void gca_diagnostics_call_diagnostics (
    GcaDiagnostics *proxy,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean gca_diagnostics_call_diagnostics_finish (
    GcaDiagnostics *proxy,
    GVariant **out_unnamed_arg0,
    GAsyncResult *res,
    GError **error);

gboolean gca_diagnostics_call_diagnostics_sync (
    GcaDiagnostics *proxy,
    GVariant **out_unnamed_arg0,
    GCancellable *cancellable,
    GError **error);



/* ---- */

#define GCA_TYPE_DIAGNOSTICS_PROXY (gca_diagnostics_proxy_get_type ())
#define GCA_DIAGNOSTICS_PROXY(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), GCA_TYPE_DIAGNOSTICS_PROXY, GcaDiagnosticsProxy))
#define GCA_DIAGNOSTICS_PROXY_CLASS(k) (G_TYPE_CHECK_CLASS_CAST ((k), GCA_TYPE_DIAGNOSTICS_PROXY, GcaDiagnosticsProxyClass))
#define GCA_DIAGNOSTICS_PROXY_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GCA_TYPE_DIAGNOSTICS_PROXY, GcaDiagnosticsProxyClass))
#define GCA_IS_DIAGNOSTICS_PROXY(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), GCA_TYPE_DIAGNOSTICS_PROXY))
#define GCA_IS_DIAGNOSTICS_PROXY_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GCA_TYPE_DIAGNOSTICS_PROXY))

typedef struct _GcaDiagnosticsProxy GcaDiagnosticsProxy;
typedef struct _GcaDiagnosticsProxyClass GcaDiagnosticsProxyClass;
typedef struct _GcaDiagnosticsProxyPrivate GcaDiagnosticsProxyPrivate;

struct _GcaDiagnosticsProxy
{
  /*< private >*/
  GDBusProxy parent_instance;
  GcaDiagnosticsProxyPrivate *priv;
};

struct _GcaDiagnosticsProxyClass
{
  GDBusProxyClass parent_class;
};

GType gca_diagnostics_proxy_get_type (void);

void gca_diagnostics_proxy_new (
    GDBusConnection     *connection,
    GDBusProxyFlags      flags,
    const gchar         *name,
    const gchar         *object_path,
    GCancellable        *cancellable,
    GAsyncReadyCallback  callback,
    gpointer             user_data);
GcaDiagnostics *gca_diagnostics_proxy_new_finish (
    GAsyncResult        *res,
    GError             **error);
GcaDiagnostics *gca_diagnostics_proxy_new_sync (
    GDBusConnection     *connection,
    GDBusProxyFlags      flags,
    const gchar         *name,
    const gchar         *object_path,
    GCancellable        *cancellable,
    GError             **error);

void gca_diagnostics_proxy_new_for_bus (
    GBusType             bus_type,
    GDBusProxyFlags      flags,
    const gchar         *name,
    const gchar         *object_path,
    GCancellable        *cancellable,
    GAsyncReadyCallback  callback,
    gpointer             user_data);
GcaDiagnostics *gca_diagnostics_proxy_new_for_bus_finish (
    GAsyncResult        *res,
    GError             **error);
GcaDiagnostics *gca_diagnostics_proxy_new_for_bus_sync (
    GBusType             bus_type,
    GDBusProxyFlags      flags,
    const gchar         *name,
    const gchar         *object_path,
    GCancellable        *cancellable,
    GError             **error);


/* ---- */

#define GCA_TYPE_DIAGNOSTICS_SKELETON (gca_diagnostics_skeleton_get_type ())
#define GCA_DIAGNOSTICS_SKELETON(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), GCA_TYPE_DIAGNOSTICS_SKELETON, GcaDiagnosticsSkeleton))
#define GCA_DIAGNOSTICS_SKELETON_CLASS(k) (G_TYPE_CHECK_CLASS_CAST ((k), GCA_TYPE_DIAGNOSTICS_SKELETON, GcaDiagnosticsSkeletonClass))
#define GCA_DIAGNOSTICS_SKELETON_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GCA_TYPE_DIAGNOSTICS_SKELETON, GcaDiagnosticsSkeletonClass))
#define GCA_IS_DIAGNOSTICS_SKELETON(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), GCA_TYPE_DIAGNOSTICS_SKELETON))
#define GCA_IS_DIAGNOSTICS_SKELETON_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GCA_TYPE_DIAGNOSTICS_SKELETON))

typedef struct _GcaDiagnosticsSkeleton GcaDiagnosticsSkeleton;
typedef struct _GcaDiagnosticsSkeletonClass GcaDiagnosticsSkeletonClass;
typedef struct _GcaDiagnosticsSkeletonPrivate GcaDiagnosticsSkeletonPrivate;

struct _GcaDiagnosticsSkeleton
{
  /*< private >*/
  GDBusInterfaceSkeleton parent_instance;
  GcaDiagnosticsSkeletonPrivate *priv;
};

struct _GcaDiagnosticsSkeletonClass
{
  GDBusInterfaceSkeletonClass parent_class;
};

GType gca_diagnostics_skeleton_get_type (void);

GcaDiagnostics *gca_diagnostics_skeleton_new (void);


G_END_DECLS

#endif /* __GCA_DIAGNOSTICS_H__ */