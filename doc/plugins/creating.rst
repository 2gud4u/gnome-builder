##########################
Creating Your First Plugin
##########################

Plugins consist of two things.
First, a meta-data file describing the plugin which includes things like a name, the author, and where to find the plugin.
Second, the plugin code which can take the form of a shared library or python module.

Builder supports writing plugins in C, C++, Vala, or Python.
We will be using Python for our examples in this tutorial because it is both succinct and easy to get started with.

First, we will look at our plugin meta-data file.
The file should have the file-suffix of ".plugin" and it's format is familiar.
It starts with a line containing "[Plugin]" indicating this is plugin meta-data.
Then it is followed by a series of "Key=Value" key-pairs.

.. code-block:: ini

   # my_plugin.plugin
   [Plugin]
   Name=My Plugin
   Loader=python3
   Module=my_plugin
   Author=Angela Avery

Now we can create a simple plugin that will print "hello" when Builder starts and "goodbye" when Builder exits.

.. code-block:: python3

   # my_plugin.py

   import gi

   from gi.repository import GObject
   from gi.repository import Ide

   class MyAppAddin(GObject.Object, Ide.ApplicationAddin):

       def do_load(self, application):
           print("hello")

       def do_unload(self, application):
           print("goodbye")

In the python file above, we define a new extension called ``MyAppAddin``.
It inherits from ``GObject.Object`` (which is our base object) and implements the interface ``Ide.ApplicationAddin``.
We wont get too much into objects and interfaces here, but the plugin manager uses this information to determine when and how to load our extension.

The ``Ide.ApplicationAddin`` requires that two methods are implemented.
The first is called ``do_load`` and is executed when the extension should load.
And the second is called ``do_unload`` and is executed when the plugin should cleanup after itself.
Each of the two functions take a parameter called ``application`` which is an ``Ide.Application`` instance.

Loading our Plugin
==================

Now place the two files in ``~/.local/share/gnome-builder/plugins`` as ``my_plugin.plugin`` and ``my_plugin.py``.
If we run Builder from the command line, we should see the output from our plugin!

.. code-block:: sh

   [angela@localhost ~] gnome-builder
   hello

Now if we close the window, we should see that our plugin was unloaded.

.. code-block:: sh

   [angela@localhost ~] gnome-builder
   hello
   goodbye

Next, continue on to learn about other interfaces you can implement in Builder to extend it's features!
