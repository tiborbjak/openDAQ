import tkinter as tk
from tkinter import ttk

from .dialog import Dialog


class AuthorizeModuleDialog(Dialog):
    ALWAYS = 'always'
    YES = 'yes'
    NO = 'no'

    def __init__(self, parent, context, module_path, title, reason, **kwargs):
        Dialog.__init__(self, parent, title, context, **kwargs)
        self.result = self.NO
        self.resizable(False, False)

        ttk.Label(self, text=reason, wraplength=int(500 * context.dpi_factor)).grid(
            row=0, column=0, columnspan=3, sticky=tk.W, pady=(5, 0))
        ttk.Label(self, text=module_path, wraplength=int(500 * context.dpi_factor)).grid(
            row=1, column=0, columnspan=3, sticky=tk.W, padx=(10, 0), pady=5)
        ttk.Label(self, text='Load it anyway?').grid(
            row=2, column=0, columnspan=3, sticky=tk.W, pady=(0, 10))

        ttk.Button(self, text='Always', command=lambda: self.finish(self.ALWAYS)).grid(row=3, column=0, padx=5, pady=5)
        ttk.Button(self, text='Yes', command=lambda: self.finish(self.YES)).grid(row=3, column=1, padx=5, pady=5)
        no_button = ttk.Button(self, text='No', command=lambda: self.finish(self.NO))
        no_button.grid(row=3, column=2, padx=5, pady=5)

        # closing the window or pressing Escape means "No"
        self.protocol('WM_DELETE_WINDOW', lambda: self.finish(self.NO))
        self.bind('<<DialogReady>>', lambda event: no_button.focus_set())

    def finish(self, result):
        self.result = result
        self.close()

    def ask(self):
        self.show()
        return self.result
