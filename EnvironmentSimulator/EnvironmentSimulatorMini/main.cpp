#include <iostream>
#include <string>
#include <random>
#include <thread>
#include <chrono>

#include "ScenarioReader.hpp"
#include "Catalogs.hpp"
#include "RoadNetwork.hpp"
#include "Entities.hpp"
#include "Init.hpp"
#include "Story.hpp"
#include "ScenarioEngine.hpp"
#include "ScenarioGateway.hpp"

#include "viewer.hpp"
#include "RoadManager.hpp"
#include "RubberbandManipulator.h"

double deltaSimTime;

static const double stepSize = 0.01;
static const double maxStepSize = 0.01;
static const double minStepSize = 0.001;
static const bool freerun = true;
static std::mt19937 mt_rand;

int main(int argc, char *argv[])
{	
	mt_rand.seed(time(0));

	// Simulation constants
	double endTime = 100;
	double simulationTime = 0;
	double timeStep = 1;

	// use an ArgumentParser object to manage the program arguments.
	osg::ArgumentParser arguments(&argc, argv);

	arguments.getApplicationUsage()->setApplicationName(arguments.getApplicationName());
	arguments.getApplicationUsage()->setDescription(arguments.getApplicationName());
	arguments.getApplicationUsage()->setCommandLineUsage(arguments.getApplicationName() + " [options]\n");
	arguments.getApplicationUsage()->addCommandLineOption("--osc <filename>", "OpenSCENARIO filename");

	if (arguments.argc() < 2)
	{
		arguments.getApplicationUsage()->write(std::cout, 1, 80, true);
		return -1;
	}

	std::string oscFilename;
	arguments.read("--osc", oscFilename);

	// Initialization
	ScenarioReader scenarioReader;
	Catalogs catalogs;
	RoadNetwork roadNetwork;
	Entities entities;
	Init init;
	std::vector<Story> story;

	// Load and parse data
	scenarioReader.loadXmlFile(oscFilename.c_str());
	scenarioReader.parseParameterDeclaration();
	scenarioReader.parseRoadNetwork(roadNetwork);
	scenarioReader.parseCatalogs(catalogs);
	scenarioReader.parseEntities(entities);
	scenarioReader.parseInit(init);
	scenarioReader.parseStory(story);

	char* scenegraphFilename = &roadNetwork.SceneGraph.filepath[0];
	char* odrFilename = &roadNetwork.Logics.filepath[0];

	viewer::Viewer *viewer = new viewer::Viewer(roadmanager::Position::GetOpenDrive(), scenegraphFilename, arguments);

	// Init road manager
	if (!roadmanager::Position::LoadOpenDrive(odrFilename))
	{
		printf("Failed to load ODR %s\n", odrFilename);
		return -1;
	}
	roadmanager::OpenDrive *odrManager = roadmanager::Position::GetOpenDrive();


	// Print loaded data
	entities.printEntities();
	init.printInit();
	story[0].printStory();

	// ScenarioEngine
	ScenarioEngine scenarioEngine(catalogs, entities, init, story, simulationTime);
	scenarioEngine.initRoutes();
	scenarioEngine.initCars();
	scenarioEngine.initInit();
	scenarioEngine.initConditions();

	// ScenarioGateway
	ScenarioGateway & scenarioGateway = scenarioEngine.getScenarioGateway();

	//  Create cars for visualization
	for (int i = 0; i < scenarioEngine.cars.getNum(); i++)
	{
		int carModelID = (double(viewer->carModels_.size()) * mt_rand()) / (std::mt19937::max)();
		viewer->AddCar(carModelID);
	}

	__int64 now, lastTimeStamp = 0;
	double simTime = 0;

	while (!viewer->osgViewer_->done())
	{
		// Get milliseconds since Jan 1 1970
		now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		deltaSimTime = (now - lastTimeStamp) / 1000.0;  // step size in seconds
		lastTimeStamp = now;

		if (deltaSimTime > maxStepSize) // limit step size
		{
			deltaSimTime = maxStepSize;
		}
		else if (deltaSimTime < minStepSize)  // avoid CPU rush, sleep for a while
		{
			std::this_thread::sleep_for(std::chrono::milliseconds((int)(1000 * (minStepSize - deltaSimTime))));
			deltaSimTime = minStepSize;
		}

		// Time operations
		simTime = simTime + deltaSimTime;
		scenarioEngine.setSimulationTime(simTime);
		scenarioEngine.setTimeStep(deltaSimTime);

		////// Gateway
		//int track_id = 2;
		//int lane_id = 1;
		//double s = 200 - 2*simTime*simTime;
		//double offset = 0;
		//roadmanager::Position p(track_id, lane_id, s, offset);
		//scenarioGateway.setExternalCarPosition("Ego", p);

		// ScenarioEngine
		scenarioEngine.stepObjects(deltaSimTime);
		scenarioEngine.checkConditions();
		scenarioEngine.executeActions();


		// Visualize cars
		for (int i = 0; i<scenarioEngine.cars.getNum(); i++)
		{
			viewer::CarModel *car = viewer->cars_[i];
			roadmanager::Position pos = scenarioEngine.cars.getPosition(i);

			car->SetPosition(pos.GetX(), pos.GetY(), pos.GetZ());
			car->SetRotation(pos.GetH(), pos.GetR(), pos.GetP());
		}
		// aheadof test
		printf("%s (%.2f, %.2f) is ahead of %s (%.2f, %.2f) : %.2f\n", 
			scenarioEngine.cars.getCarPtrByIdx(0)->getObjectName().c_str(),
			scenarioEngine.cars.getPosition(0).GetX(), scenarioEngine.cars.getPosition(0).GetY(),
			scenarioEngine.cars.getCarPtrByIdx(1)->getObjectName().c_str(),
			scenarioEngine.cars.getPosition(1).GetX(), scenarioEngine.cars.getPosition(1).GetY(),
			scenarioEngine.cars.getPosition(0).getRelativeDistance(scenarioEngine.cars.getPosition(1)));
		
		viewer->osgViewer_->frame();
	}

	return 1;
}

