#pragma once

#include <decaf/lang/Thread.h>
#include "ActiveMQ/ActiveMQProducer.hxx"
#include <decaf/util/concurrent/CountDownLatch.h>
#include <simgear\structure\subsystem_mgr.hxx>
#include <simgear/props/props.hxx>
#include "Main/fg_props.hxx"
#include <iostream>
#include <sstream>
//#include <windows.h>

class ActiveMQProducerSubsystem : public SGSubsystem
{
public:
	ActiveMQProducerSubsystem() {m_producer = NULL; m_routeRoot = NULL;}
	virtual ~ActiveMQProducerSubsystem()
	{
		// Close and clean up the producer
		m_producer->Close();
		//m_producerThread->join();

		if(m_producer != NULL)
			delete m_producer;

		m_producer = NULL;

		//delete m_producerThread;

		activemq::library::ActiveMQCPP::shutdownLibrary();
	}

	void ActiveMQProducerSubsystem::init()
	{
		activemq::library::ActiveMQCPP::initializeLibrary();

		// Create the producer
		m_producer = new ActiveMQProducer("failover://(tcp://localhost:61616)", "TEST.FOO", false);

		// Libraries that the activemq producer uses are not thread safe
		// Need to atleast start the producer in its own thread
		Thread producerThread(m_producer);
		producerThread.start();
		//m_producer->run();
		producerThread.join();

		// Add the multiplayer parent node
		m_multiplayer = fgGetNode("/", true)->addChild("multiplayer");

		 // get a pointer to the node that holds the desired property
		m_lat = fgGetNode("position/latitude-deg", true);
		m_long = fgGetNode("position/longitude-deg", true);
		m_altitude = fgGetNode("position/altitude-ft", true);
		m_totalFuel = fgGetNode("consumables/fuel/total-fuel-gals", true);
		m_percentFuelLevel = fgGetNode("consumables/fuel/total-fuel-norm", true);
		m_currentTime = fgGetNode("instrumentation/clock/local-short-string", true);
		m_orientationDeg = fgGetNode("orientation/heading-deg", true);
		m_airspeed = fgGetNode("velocities/airspeed-kt", true);
		m_temperatureF = fgGetNode("environment/temperature-degf", true);
		m_windSpeedKnots = fgGetNode("environment/wind-speed-kt", true);
		m_windHeading = fgGetNode("environment/wind-from-heading-deg", true);
		m_weatherScenario = fgGetNode("environment/weather-scenario", true);

		// Get the route root property node
		m_routeRoot = fgGetNode("autopilot/route-manager", true);

		m_lastMessageSent = SGTimeStamp::now();
	}

	void ActiveMQProducerSubsystem::reinit()
	{
		// Close and clean up the producer
		m_producer->Close();

		if(m_producer != NULL)
			delete m_producer;

		//delete m_producerThread;

		// Create the producer
		m_producer = new ActiveMQProducer("failover://(tcp://localhost:61616)", "TEST.FOO", false);

		// Libraries that the activemq producer uses are not thread safe
		// Need to atleast start the producer in its own thread
		Thread producerThread(m_producer);
		producerThread.start();
		//m_producer->run();
		producerThread.join();
	}

	void ActiveMQProducerSubsystem::bind()
	{
	}

	void ActiveMQProducerSubsystem::unbind()
	{
	}

	void ActiveMQProducerSubsystem::update(double dt)
	{
		// If we haven't sent an update in 300 milliseconds,
		// send another update
		if((SGTimeStamp::now() - m_lastMessageSent).toMSecs() > 3000)
		{
			//
			// Get appropriate values from the property nodes
			//

			// Latitude, Longitude, and Altitude
			double pos_lat = m_lat->getDoubleValue();
			double pos_long = m_long->getDoubleValue();
			double pos_alt = m_altitude->getDoubleValue();

			// Aircraft heading
			double orientationDeg = m_orientationDeg->getDoubleValue();

			// Airspeed
			double airspeedKnots = m_airspeed->getDoubleValue();

			// Current local time
			std::string time_current = m_currentTime->getStringValue();

			// Fuel levels
			double fuel_total = m_totalFuel->getDoubleValue();
			double fuel_percentLevel = m_percentFuelLevel->getDoubleValue();
			m_totalFuelCapacity = fuel_total / fuel_percentLevel;

			// Format the message
			std::ostringstream message_stream;
			message_stream << "playername Player" << ",";
			message_stream << "latitude-deg" << " " << pos_lat << ","; 
			message_stream << "longitude-deg" << " " << pos_long << ",";
			message_stream << "altitude" << " " << pos_alt << ",";
			message_stream << "heading-deg" << " " << orientationDeg << ",";
			message_stream << "airspeed-kt" << " " << airspeedKnots << ",";
			message_stream << "local-short-string" << " " << time_current << ",";
			message_stream << "total-fuel-gals" << " " << fuel_total << ",";
			message_stream << "total-fuel-capacity" << " " << m_totalFuelCapacity << "\n";
			message_stream << "\n";

			// Get info for mutliplayer message
			SGPropertyNode *multiplayerRoot = fgGetNode("multiplayer", true);
			if(multiplayerRoot->nChildren() > 0)
			{
				std::string playerName = "";
				double latitude = 0.0;
				double longitude = 0.0;
				double alt = 0.0;
				double speed = 0.0;
				float angle = 0.0;
				for(int i = 0; i < multiplayerRoot->nChildren(); i++)
				{
					SGPropertyNode *n = multiplayerRoot->getChild(i);
					playerName = n->getNameString();
					latitude = n->getChild("position")->getChild("latitude-deg")->getDoubleValue();
					longitude = n->getChild("position")->getChild("longitude-deg")->getDoubleValue();
					alt = n->getChild("position")->getChild("altitude")->getDoubleValue();
					angle = n->getChild("orientation")->getDoubleValue();
					speed = n->getChild("speed")->getDoubleValue();
					message_stream << "playername " << playerName << ",";
					message_stream << "latitude-deg" << " " << latitude << ",";
					message_stream << "longitude-deg" << " " << longitude << ",";
					message_stream << "altitude" << " " << alt << ",";
					message_stream << "heading-deg" << " " << angle << ",";
					message_stream << "airspeed-kt" << " " << speed << "\n";
					message_stream << "\n";
				}
			}

			// Environment info
			double tempF = m_temperatureF->getDoubleValue();
			double windSpeed = m_windSpeedKnots->getDoubleValue();
			double windHeading = m_windHeading->getDoubleValue();
			std::string weatherScenario = m_weatherScenario->getStringValue();

			message_stream << "Environment,";
			message_stream << "temperature-degf" << " " << tempF << ",";
			message_stream << "wind-speed-kt" << " " << windSpeed << ",";
			message_stream << "wind-from-heading-deg" << " " << windHeading << ",";
			message_stream << "weather-scenario" << " " << weatherScenario << "\n";
			message_stream << "\n";

			message_stream << "END";

			// Send message
			std::string msg = message_stream.str();
			m_producer->SendMessage(msg);

			// Update timer
			m_lastMessageSent = SGTimeStamp::now();
		}
	}

	int ActiveMQProducerSubsystem::StephaniesFunction()
	{
		return 0;
	}

private:
	//! Our ActiveMQ Producer
	ActiveMQProducer *m_producer;

	//! ActiveMQProducer thread
	//Thread *m_producerThread;

	//! Timer
	SGTimeStamp m_lastMessageSent;

	//! Total capacity for the fuel tanks on the plane
	double m_totalFuelCapacity;

	//
	// Property Nodes
	//

	//! Pointer to the property node that holds the latitude
	SGPropertyNode *m_lat;

	//! Pointer to the property node that holds the longitude
	SGPropertyNode *m_long;

	//! Pointer to the property node that holds the altitude
	SGPropertyNode *m_altitude;

	//! Pointer to the property node that holds the total fuel in the tanks in gallons (US)
	SGPropertyNode *m_totalFuel;

	//! Pointer to the property node that holds the percentage of fuel we have left
	SGPropertyNode *m_percentFuelLevel;

	//! Pointer to the property node that holds the in-game time
	SGPropertyNode *m_currentTime;

	//! Pointer to the property node that holds the aircraft orientation in degrees
	SGPropertyNode *m_orientationDeg;

	//! Pointer to the property node that holds the aircraft speed in knots
	SGPropertyNode *m_airspeed;

	//! Pointer to the property node that holds the temperature in degrees fahrenheit
	SGPropertyNode *m_temperatureF;

	//! Pointer to the property node that holds the wind speed in knots
	SGPropertyNode *m_windSpeedKnots;

	//! Pointer to the property node that holds the wind direction in degrees
	SGPropertyNode *m_windHeading;

	//! Pointer to the property node that holds the weather scenario (Fair, Rain, Snow, etc.)
	SGPropertyNode *m_weatherScenario;

	//! Pointer to the root autopilot property node
	SGPropertyNode *m_routeRoot;

	//! Root of the created multiplayer node
	SGPropertyNode *m_multiplayer;
};