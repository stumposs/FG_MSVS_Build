#pragma once

#include <decaf/lang/Thread.h>
#include "ActiveMQ/ActiveMQProducer.hxx"
#include <decaf/util/concurrent/CountDownLatch.h>
#include <simgear\structure\subsystem_mgr.hxx>
#include <simgear/props/props.hxx>
#include "Main/fg_props.hxx"
#include<iostream>


class ActiveMQProducerSubsystem : public SGSubsystem
{
public:
	ActiveMQProducerSubsystem() {m_producer = NULL;}
	virtual ~ActiveMQProducerSubsystem()
	{
		m_producer->Close();

		activemq::library::ActiveMQCPP::shutdownLibrary();

		if(m_producer != NULL)
			delete m_producer;

		m_producer = NULL;
		//m_position = NULL;
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
		 // m_position = fgGetNode("position/latitude-deg", true);
		  
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
		//double pos = m_position->getDoubleValue();
		m_producer->SendMessage("WOOOOORK!");
	}

private:
	ActiveMQProducer *m_producer;
	//SGPropertyNode *m_position;
};