Multitap Delay AU notes (August, 2003)
--------------------------------------

The Multitap's custom view is now built in a separate binary, and weak-linked from the AU.  The AU has a hard link to the ViewComponent's binary, and uses the bundle identifier of the AU's bundle to locate where that view code lives

Install the built view component (MultitapAUView) in MultitapAU.component/Contents/MacOS (and of course install or symlink MultitapAU.component into a Library/Audio/Plug ins/Components directory)

Then run your host app, and you should see the AU, and get its custom view