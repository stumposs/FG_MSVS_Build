#pragma once

#include <decaf/lang/Thread.h>
#include "ActiveMQ/ActiveMQConsumer.h"
#include <decaf/util/concurrent/CountDownLatch.h>
#include <simgear\structure\subsystem_mgr.hxx>

class ActiveMQConsumerSubsystem : public SGSubsystem
{
public:
	ActiveMQConsumerSubsystem() {m_consumer = NULL; m_consumerThread = NULL;}
	virtual ~ActiveMQConsumerSubsystem()
	{
		m_consumer->close();
		m_consumerThread->join();

		activemq::library::ActiveMQCPP::shutdownLibrary();

		if(m_consumer != NULL)
			delete m_consumer;

		m_consumer = NULL;
	}

	void init()
	{
		activemq::library::ActiveMQCPP::initializeLibrary();

		m_consumer = new ActiveMQConsumer("failover://(tcp://localhost:61616)");
		
		m_consumerThread = new Thread(m_consumer);
		m_consumerThread->start();

		//m_consumer->run();

		// Wait for the consumer to indicate that it's ready to go
		m_consumer->waitUntilReady();
	}

	void reinit()
	{
		//m_consumer->close();

		/*if(m_consumer != NULL)
			delete m_consumer;

		m_consumer = new ActiveMQConsumer("failover://(tcp://localhost:61616)");

		Thread consumerThread(m_consumer);
		consumerThread.start();*/

		//m_consumer->run();

		// Wait for the consumer to indicate that it's ready to go
		//m_consumer->waitUntilReady();
	}

	void bind()
	{
	}

	void unbind()
	{
	}

	void update(double dt)
	{
	}

private:
	//! Our ActiveMQ Consumer
	ActiveMQConsumer *m_consumer;

	//! ActiveMQConsumer thread
	Thread *m_consumerThread;

};
