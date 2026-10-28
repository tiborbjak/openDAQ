import re
import tkinter as tk
from tkinter import ttk

from ..app_context import AppContext


class Dialog(tk.Toplevel):
    def __init__(self, parent, title, context: AppContext, **kwargs):
        tk.Toplevel.__init__(self, parent, **kwargs)
        self.withdraw()  # keep hidden until positioned to avoid top-left flash
        self.title(title)
        self.parent = parent
        self.context = context

        self.configure(padx=10, pady=5)
        self.attributes('-topmost', False)
        self.transient(parent)
        
        # Bind Escape key to close the dialog
        self.bind("<Escape>", lambda event: self.close())

    def wm_geometry(self, newGeometry=None):
        # remember the size set by a subclass, a hidden window reports 1x1 until it is shown
        size = re.match(r'(\d+)x(\d+)', newGeometry or '')
        if size:
            self._requested_size = int(size.group(1)), int(size.group(2))
        return tk.Toplevel.wm_geometry(self, newGeometry)

    geometry = wm_geometry

    def center_window(self):
        """Positions the dialog over the main window while it is hidden. Moving a shown window makes Tk wait
        for the window manager to confirm the move, up to 2s when it does not."""
        self.update_idletasks()  # ensure the requested size is computed while hidden
        w, h = getattr(self, '_requested_size', None) or (self.winfo_reqwidth(), self.winfo_reqheight())
        main_window = self.parent.winfo_toplevel()
        x = main_window.winfo_rootx() + main_window.winfo_width() // 2 - w // 2
        y = main_window.winfo_rooty() + main_window.winfo_height() // 2 - h // 2
        self.geometry(f'+{x}+{y}')

    def initial_update(self):
        pass

    def show(self):
        self.initial_update()   # populate content while dialog is still hidden
        self.center_window()    # position before showing
        self.deiconify()        # appear fully populated in one step
        self.update_idletasks()
        self.event_generate('<<DialogReady>>')
        self.focus_set()
        self.grab_set()         # Prevent interaction with other windows
        self.wait_window(self)

    def show_modeless(self):
        self.initial_update()
        self.center_window()
        self.deiconify()
        self.update_idletasks()
        self.event_generate('<<DialogReady>>')
        self.focus_set()
        
    def close(self):
        self.destroy()
