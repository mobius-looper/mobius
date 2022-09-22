Fri Mar 16 21:47:21 2012

Logic
  This has a logic projects used for testing mobius.
    - it is not checked in to source control, move these out
      into a different reposoitory?

pkgmaker
  This has the PackageMaker project and support files.

AUMobius.exp
  This is used by makefile.mac when building the AU plugin. 
  I think the reason this is down here and all the other AU files
  are up a level is because .exp means something different on Windows
  and I was having CVS or SVN problems (assumed binary?).

bundles
  Support files necessary to create the three bundles: app, vst, and 
  component.  This diretory will also contain the build artifacts.

xcode
  Xcode projects.

