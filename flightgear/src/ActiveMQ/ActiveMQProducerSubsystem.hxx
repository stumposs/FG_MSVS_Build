#pragma once

#include <decaf/lang/Thread.h>
#include "ActiveMQ/ActiveMQProducer.hxx"
#include <decaf/util/concurrent/CountDownLatch.h>
#include <simgear\structure\subsystem_mgr.hxx>
#include <simgear/props/props.hxx>
#include "Main/fg_props.hxx"
#include <iostream>
#include <sstream>


class ActiveMQProducerSubsystem : public SGSubsystem
{
public:
	ActiveMQProducerSubsystem() {m_producer = NULL; m_lat = NULL; m_long = NULL;}
	virtual ~ActiveMQProducerSubsystem()
	{
		m_producer->Close();

		activemq::library::ActiveMQCPP::shutdownLibrary();

		if(m_producer != NULL)
			delete m_producer;

		m_producer = NULL;
		m_lat = NULL;
		m_long = NULL;
	}

	void ActiveMQProducerSubsystem::init()
	{
		activemq::library::ActiveMQCPP::initializeLibrary();
		m_producer = new ActiveMQProducer("failover://(tcp://localhost:61616)", 20, "TEST.FOO", false);
		Thread producerThread(m_producer);
		producerThread.start();
		//m_producer->run();
		producerThread.join();
		 // get a pointer to the node that holds the desired property
		m_lat = fgGetNode("position/latitude-deg", true);
		m_long = fgGetNode("position/longitude-deg", true);
		  
		  /*
		positionLat = props->getNode("position/latitude-deg", true);
		positionAlt = props->getNode("position/altitude-ft", true);
		*/
	}

	void ActiveMQProducerSubsystem::reinit()
	{
		//m_producer->Close();
		//m_producer->run();
	}

	void ActiveMQProducerSubsystem::bind()
	{
	}

	void ActiveMQProducerSubsystem::unbind()
	{
	}

	void ActiveMQProducerSubsystem::update(double dt)
	{
		double pos_lat = m_lat->getDoubleValue();
		double pos_long = m_long->getDoubleValue();
		std::ostringstream pos_s;
		pos_s << pos_lat << " " << pos_long;
		std::string pos = pos_s.str();
		m_producer->SendMessage(pos);
	}

private:
	ActiveMQProducer *m_producer;
	SGPropertyNode *m_lat;
	SGPropertyNode *m_long;
};