Shroud Plugin for CryEngine SDK
=====================================

Shroud Integration for Cryengine FreeSDK

Installation / Integration
--------------------------

* Download Plugin_SDK source code from https://github.com/hendrikp/Plugin_SDK

  Follow installation instructions; unzip it in CryEngine FreeSDK folder, inside Code, right next to CryEngine, Game, etc.  Compile it.  Apply FreeSDK code changes to get it to invoke Plugin_SDK.  Verify that you got everything right. :-)

* Download Shroud Studio Basic from http://community.cloak-works.com/index.php/files/category/1-cloakworks-products/

  Install is anywhere you like it.  This is the tool where you'll be authoring the simulation.

* Download Shroud Runtime library (ShroudLibs_PC.exe)

  Install it to your CryEngine FreeSDK Code/ folder, leave the folder name 'Shroud Runtime'

* Download this Plugin_Shroud, and place it inside your Code/ Folder, right next to the previous folders.

  At the end, your Code/ contents should look similar to this:
```
 Volume in drive C has no label.
 Volume Serial Number is E8A7-3F41

 Directory of C:\ws\thegame\Code

18/08/2013  02:24 PM    <DIR>          .
18/08/2013  02:24 PM    <DIR>          ..
18/08/2013  02:23 PM    <DIR>          CryEngine
18/08/2013  02:24 PM    <DIR>          Game
18/08/2013  02:24 PM    <DIR>          Plugin_SDK
19/08/2013  11:03 PM    <DIR>          Plugin_Shroud
18/08/2013  02:24 PM    <DIR>          SDKs
18/08/2013  02:24 PM    <DIR>          Shroud Runtime
18/08/2013  02:24 PM    <DIR>          Solutions
```
  (plus whatever else you may have installed there)

  Open VS2010, and compile.  On success run Editor.  Avoid Debug/Win32 (see known issues for more info)


Usage
-----

1. Export the object

   Create object in your 3d app.  Export it as .cfg.  Note -- both character simulated objects
   and GeomEntity will be simulated from this .cfg.

   If simulating character, create new attachment (note its name), attach .cfg to it. To Bone: <your root bone>

   Plugin and Shroud Runtime will take care of skinning it to the animation.

   Export the same object as .fbx

2. In Shroud Studio, import this .fbx, and follow Cloak-Works.com tutorials on how to author simulation.

   Export the file, leave it as 'xml' for the moment (see known issues below as to why binary doesn't work)

   TODO: add note about how to set coords x/y/z correctly.

3. In Editor, add this cgf as Geom Entity.  Open its flowgraph.  Create Start node, and create
   Shroud_Plugin/StaticObjectCloth.

   AssignGraphEntity to entityId, assign Start to Activate, pull up and navigate to .cwf file you exported in step 3.

4. CTRL-G and watch your simulation.

5. Character simulation works exactly the same, the only exception is that you need to specify the name of the attachment that you noted in step 1.

Flownodes
---------

* ShroudPlugin::StaticObjectCloth

  This node is to be used on Geom Entity, to activate shroud simulation.
```
Inputs:
EntityId (entity to simulate)
Activate (enable shroud)
sShroudFile (filename of the export from Shroud Studio -- can be inside a .pak)
```

* ShroudPlugin::CharacterCloth
```
Inputs:
EntityId (entity to simulate)
Activate (enable shroud)
sShroudFile (filename of the export from Shroud Studio -- can be inside a .pak)
sAttName (name of the character attachment to simulate)
```

Notes
-----

* Current implementation turns off simulation for objects that were not drawn 'previous or one before' frame.
  ("or one before" seems to be very important when ocean is being drawn, order of 'previous frame' is affected of separate frame drawing the ocean -- at least that's my interpretation of behavour; could be mistaken)

  This is the cheapest and quickest way to establish that an object is not on screen (by comparing object's pRenderNode->GetDrawFrame, and current gEnv->pRenderer->GetFrameID()

* A lot of the code was based on OpenGL Example Project 1.1 from cloak-works.com; many thanks for this!

* Further optimization based on distance from camera is possible.

* Further optimization to multi-thread vertex copies is possible.

* Collision with character and projectiles is currently not implemented.  Collision with terrain is possible but currently not implemented (eg: workout angle of the terrain right below the pivot, create 'plane' collider that matches that position.
  Currently, it is easy to simulate collision with terrain by creating Shroud Studio plane collider at the correct height.

* CShroudSystem.cpp is the main part of the code, so please poke around there to make it better!

Known issues
------------

* Vertex colors are not propagated and used.  Unable to use shaders that depend on it.

* One submaterial per simulated object.  Shroud doesn't support simulations with multiple submaterials.

* Release/x64, Debug/x64, Release/Win32 -- all compile correctly.  Debug/Win32 for some reason, plays simulation at half the speed.  I haven't had the time to identify what causes the problem.

* Plugin supports any number of colliders; Shroud Studio Professional is really needed for some complex character cloth simulations.

* Shroud supports LOD's, however this plugin doesn't support them (yet).

* Shroud supports instancing of the same simulation, however this plugin (currently) ignores this and creates new simulations multiple times.

* If you have a brush on the scene with the same .cgf, it will vanish when simulation on another Geom Entity starts.

* Collision with the world doesn't work

* Resizing of shroud simulated objects is currently not supported by Shroud, nor by this plugin; however it is possible to apply this translation during copy back from shroud.

* Large chunk of code was lazily copied from CShroudWrapper::ActivateCharacterCloth to CShroudWrapper::ActivateStatObjCloth, without following correct coding practices.  This really needs refactoring.



Disclaimers
-----------
The code and plugin are not affiliated with, endorsed by, or supported by CloakWorks. All of the Shroud APIs, libs, docs, and other IP associated with Shroud are the sole property of CloakWorks.


Shroud OpenGL Example License
-----------------------------
Shroud Example Code License

These files are part of the Shroud(TM) Example Projects.  For more information please visit http://www.cloak-works.com

With the exception of the Shroud SDK (which is subject to a separate license) and the contents of the ExternalAPIs folder (which are subject to their own licenses), this code is subject to the following terms (MIT License):

Copyright (C) 2012 CloakWorks Inc.

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
