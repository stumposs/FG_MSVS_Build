FG_MSVS_Build
=============

Install Instructions:

Required Software:
  Windows 7
  CMake GUI
  Microsoft Visual Studio 2010
  Microsoft Windows SDK

Before we do anything, we need to set up the directory tree cmake is expecting. (A sample file structure is available on this git repo.) It should look like this:
  MSVC_3RDPARTY_ROOT /
    3rdParty.x64 /
      bin /         (included in downloaded file)
      include /     (included in downloaded file)
      lib /         (included in downloaded file)
    boost_1_53_0 /
      boost /       (included in downloaded file)
    install /
      msvc100-64 /
        OpenSceneGraph /
          bin /      (installed later)
          include /  (installed later)
          lib /      (installed later)
        SimGear /
        FlightGear /

1. If you have downloaded the structure from this git repo, download boost(1_53_0) from the boost website and place the boost file(boost_1_53_0) in the MSVC_3RDPARTY_ROOT. If not, go to: "http://wiki.flightgear.org/Building_using_CMake_-_Windows" and scroll to the "64-bits" section. Download the 64-bit package for 3rdParty. The 3rdParty package will have an older version of boost. Replace it with a newer boost(1_53_0) downloaded from the boost website.
2. If you downloaded the MSVC_3RDPARTY_ROOT from this git repo, you can skip this step. Download OpenSceneGraph 3.2.0 from the OpenSceneGraph website. Extract it and then open CMake. Open the OpenSceneGraph folder and drag the file "CMakeLists.txt" onto the CMake window. In "Where to build the binaries", add "/build" to the end. This is where the project files will appear. Click the "Configure" button and click "Yes" to the dialog box that appears. Another dialog box should appear that is asking you what generator to use for this project. Expand the drop-down list and click on "Visual Studio 10 Win 64". Click "Finish" and then "Configure". After it is done, find the variable "CMAKE_INSTALL_PREFIX" and change that to the OpenSceneGraph folder in the directory tree above. If you can't find "CMAKE_INSTALL_PREFIX", check the "Advanced" box near the top. Click "Configure" again and then click "Generate". Navigate to the folder where you built your binaries and open the OpenSceneGraph solution file. Build the OpenSceneGraph solution(this will take awhile). After that is done, build just the "INSTALL" project in the solution explorer. NOTE: Build the release version.
3. Pull the SimGear and FlightGear source code from this git repository.
4. Make separate folders to hold the builds for SimGear and FlightGear. These can be in My Documents.
5. Open CMake and drag the "CMakeLists.txt" from the SimGear folder that holds the source code. In "Where to build the binaries", change the path to the folder you just made for SimGear to hold the builds. Click "Configure" and select "Visual Studio 10 Win64" again. Click "Finish". Press the "Advanced" box near the top. Find the variable "BOOST_ROOT" and change it to "MSVC_3RDPARTY_ROOT/boost_1_53_0/". Find "CMAKE_INSTALL_PREFIX" and change it to "MSVC_3RDPARTY_ROOT/install/msvc100-64/SimGear/". Find "MSVC_3RDPARTY_ROOT" and change it to the root folder you made, MSVC_3RDPARTY_ROOT in the structure above.
6. Go to where you the folder CMake built the project files for SimGear and open the solution file. Build the solution. After it is done, right click the "INSTALL" project and select "Build" NOTE: Build the release version.
7. Download freeglut(2.8.1-1)(www.transmissionzero.co.uk/software/freeglut-devel/). Extract the freeglut folder. Place the 64-bit version of the dll file in bin/ into MSVC_3RDPARTY_ROOT/3rdParty.x64/bin/. Place the "GL" folder in the include/ directory into MSVC_3RDPARTY_ROOT/3rdParty.x64/include/. Place the 64-bit version of the library file in lib/ into MSVC_3RDPARTY_ROOT/3rdParty.x64/lib/. 8. Repeat the steps above for filling in the BOOST_ROOT, CMAKE_INSTALL_PREFIX, and MSVC_3RDPARTY_ROOT, except using flightgear directories instead of simgear. After you configure, click generate, open, and build the FlightGear solution file. After the build is done, build "INSTALL". NOTE: Build the release version. Next download "FlightGear-data.2.12.1.tar.bz2" from "ftp://ftp.kingmount.com/flightsims/flightgear/Shared/". Extract the folder(fgdata) and move it to "MSVC_3RDPARTY_ROOT/install/msvc100-64/FlightGear/". Also add the folders "scenery/" and "terrasync/" to the same directory.
9. Run "fgrun" in "MSVC_3RDPARTY_ROOT/install/msvc100-64/FlightGear/bin/". A window should pop up that says "FlightGear Launch Control 1.7.0" at the top. If not, press "Prev" at the bottom until you see it. Make sure the executable field links to "MSVC_3RDPARTY_ROOT-Backup/install/msvc100-64/FlightGear/bin/fgfs.exe", the FG_ROOT field links to "MSVC_3RDPARTY_ROOT-Backup/install/msvc100-64/FlightGear/fgdata" and the FG_SCENERY field contains "MSVC_3RDPARTY_ROOT-Backup/install/msvc100-64/FlightGear/data/Scenery", "MSVC_3RDPARTY_ROOT-Backup/install/msvc100-64/FlightGear/scenery", and "MSVC_3RDPARTY_ROOT-Backup/install/msvc100-64/FlightGear/terrasync". If the last two folders don't exist, create them.