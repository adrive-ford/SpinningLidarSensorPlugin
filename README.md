# SpinningLidarSensorPlugin
Tested to be working in UE4.17.2

## Overview
The Spinning Lidar Sensor Plugin allows a spinning lidar model to be added to your ego vehicle for collecting data from the scene. The sensor uses the default properties of the Velodyne HDL-32E.
### Note: Use of this plugin will cause the simulation to run ~100-1000 times slower than realtime on a regular Dell laptop.

## Enable Plugins in Existing Project

1. Create a folder in your project directory called "Plugins" if the folder does not already exist.

2. Copy the plugin into the "Plugins" folder of your project.

3. Delete the "Binary", "Intermediate", and "Saved" folders in the project directory and the Plugin directories.

4. Modify the .uproject file of your project so that it knows to add the plugins. The file should look something like this:
~~~
{
	"FileVersion": 3,
	"EngineAssociation": "{}",
	"Category": "",
	"Description": "",
	"Modules": [
		{
			"Name": "ScenarioGenerator",
			"Type": "Runtime",
			"LoadingPhase": "Default"
		}
	],
	"Plugins": [
		{
			"Name": "SpinningLidarSensorPlugin",
			"Enabled": true
		}
	]
}
~~~

5. Reload your Unreal project.

6. When prompted about excluding the Plugin because it cannot be found, select "No".

7. When prompted to rebuild the Project and Plugin, select "Yes".

#### Ensure that the plugins are enabled and compiled:
Go to "Edit -> Plugins". Search for the plugins and click on the "Enabled" option if it is not already checked.

If the plugins still do not appear: In the Editor, find "Windows -> Developer Tools -> Modules". In the Modules tab, search for the plugins. Click on "Recompile" for each.
