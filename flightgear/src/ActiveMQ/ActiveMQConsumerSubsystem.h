#pragma once

#include <decaf/lang/Thread.h>
#include "ActiveMQ/ActiveMQConsumer.h"
#include <decaf/util/concurrent/CountDownLatch.h>
#include <simgear\structure\subsystem_mgr.hxx>

class ActiveMQConsumerSubsystem : public SGSubsystem
{
public:
	ActiveMQConsumerSubsystem() {m_consumer = NULL; m_producerSubsystem = NULL;}
	virtual ~ActiveMQConsumerSubsystem()
	{
		m_consumer->close();

		//activemq::library::ActiveMQCPP::shutdownLibrary();

		if(m_consumer != NULL)
			delete m_consumer;

		m_consumer = NULL;
	}

	void init()
	{
		//activemq::library::ActiveMQCPP::initializeLibrary();

		m_consumer = new ActiveMQConsumer("failover://(tcp://localhost:61616)");
		
		Thread m_consumerThread(m_consumer);
		m_consumerThread.start();
		m_consumerThread.join();

		m_consumer->setProducerSubsystem(m_producerSubsystem);

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

	void setProducerSubsystem(ActiveMQProducerSubsystem *p) {m_producerSubsystem = p;}

private:
	//! Our ActiveMQ Consumer
	ActiveMQConsumer *m_consumer;

	//! The ActiveMQ Producer that sends fg info
	ActiveMQProducerSubsystem *m_producerSubsystem;
};
