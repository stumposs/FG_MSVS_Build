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
	ActiveMQProducerSubsystem() {m_producer = NULL;}
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
		//producerThread.join();

		// Add the multiplayer parent node
		m_multiplayer = fgGetNode("/", true)->addChild("multiplayer");

		 // get a pointer to the node that holds the desired property
		m_lat = fgGetNode("position/latitude-deg", true);
		m_long = fgGetNode("position/longitude-deg", true);
		m_altitude = fgGetNode("position/altitude-ft", true);
		m_totalFuel = fgGetNode("consumables/fuel/total-fuel-gals", true);
		m_percentFuelLevel = fgGetNode("consumables/fuel/total-fuel-norm", true);
		m_currentTime = fgGetNode("instrumentation/clock/local-short-string", true);
		m_batteryLevel = fgGetNode("systems/electrical/battery-charge-percent", true);

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
		//producerThread.join();
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
			// Get appropriate values from the property nodes
			double pos_lat = m_lat->getDoubleValue();
			double pos_long = m_long->getDoubleValue();
			double pos_alt = m_altitude->getDoubleValue();
			double fuel_total = m_totalFuel->getDoubleValue();
			double fuel_percentLevel = m_percentFuelLevel->getDoubleValue();
			m_totalFuelCapacity = fuel_total / fuel_percentLevel;
			std::string time_current = m_currentTime->getStringValue();
			double battery_percent = m_batteryLevel->getDoubleValue();

			// Format the message
			std::ostringstream message_stream;
			message_stream << "playername Player" << ",";
			message_stream << "latitude-deg" << " " << pos_lat << ","; 
			message_stream << "langitude-deg" << " " << pos_long << ",";
			message_stream << "altitude" << " " << pos_alt << "\n";
			/*message_stream << ID_FUEL << " " << fuel_total << " " << fuel_percentLevel << " " << m_totalFuelCapacity;
			message_stream << ID_BATTERY << " " << battery_percent;
			message_stream << ID_TIME << " " << time_current;
			message_stream << pos_lat << " " << pos_long;*/
			message_stream << "\n";

			// Send multiplayer message
			SGPropertyNode *multiplayerRoot = fgGetNode("multiplayer", true);
			if(multiplayerRoot->nChildren() > 0)
			{
				std::string playerName = "";
				double latitude = 0.0;
				double longitude = 0.0;
				for(int i = 0; i < multiplayerRoot->nChildren(); i++)
				{
					SGPropertyNode *n = multiplayerRoot->getChild(i);
					playerName = n->getNameString();
					latitude = n->getChild("position")->getChild("latitude-deg")->getDoubleValue();
					longitude = n->getChild("position")->getChild("longitude-deg")->getDoubleValue();
					message_stream << "playername: " << playerName << ",";
					message_stream << "latitude-deg" << " " << latitude << ",";
					message_stream << "longitude-deg" << " " << longitude << "\n";
					message_stream << "\n";
				}
				message_stream << "END";
			}

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

	//! Pointer to the property node that holds the battery level (%)
	SGPropertyNode *m_batteryLevel;

	SGPropertyNode *m_multiplayer;
};