#pragma once

#include <decaf/lang/Thread.h>
#include <decaf/lang/Runnable.h>
#include <decaf/util/concurrent/CountDownLatch.h>
#include <decaf/lang/Long.h>
#include <decaf/util/Date.h>
#include <activemq/core/ActiveMQConnectionFactory.h>
#include <activemq/util/Config.h>
#include <activemq/library/ActiveMQCPP.h>
#include <cms/Connection.h>
#include <cms/Session.h>
#include <cms/TextMessage.h>
#include <cms/BytesMessage.h>
#include <cms/MapMessage.h>
#include <cms/ExceptionListener.h>
#include <cms/MessageListener.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <memory>

using namespace activemq;
using namespace activemq::core;
using namespace decaf;
using namespace decaf::lang;
using namespace decaf::util;
using namespace decaf::util::concurrent;
using namespace cms;
using namespace std;
using namespace decaf::lang;

const static int MESSAGE_TYPE = 1;

class ActiveMQProducer : public Runnable
{
public:
	ActiveMQProducer(const std::string& brokerURI, const std::string& destURI, 
		bool useTopic = false, bool clientAck = false)
	{
		m_connection = NULL;
		m_session = NULL;
		m_destination = NULL;
		m_producer = NULL;
		m_useTopic = useTopic;
		m_clientAck = clientAck;
		m_brokerURI = brokerURI;
		m_destURI = destURI;
	}
	virtual ~ActiveMQProducer() {this->CleanUp();}

	void Close()
	{
		this->CleanUp();
	}

	bool SendMessage(std::string msg)
	{
		try
		{
			TextMessage *message = m_session->createTextMessage(msg);
			//message->setIntProperty("Integer", MESSAGE_TYPE);
			m_producer->send(message);
			delete message;
			return true;
		}
		catch(CMSException& e)
		{
			e.printStackTrace();
			return false;
		}
	}

	virtual void run()
	{
		try
		{
			// Create a ConnectionFactory
			auto_ptr<ActiveMQConnectionFactory> connectionFactory(
				new ActiveMQConnectionFactory(m_brokerURI));

			// Create a connection
			try
			{
				m_connection = connectionFactory->createConnection();
				m_connection->start();
			}
			catch(CMSException& e)
			{
				e.printStackTrace();
				throw e;
			}

			// Create a session
			if(m_clientAck)
			{
				m_session = m_connection->createSession(Session::CLIENT_ACKNOWLEDGE);
			}
			else
			{
				m_session = m_connection->createSession(Session::AUTO_ACKNOWLEDGE);
			}

			// Create the destination (Topic or queue)
			if(m_useTopic)
			{
				m_destination = m_session->createTopic(m_destURI);
			}
			else
			{
				m_destination = m_session->createQueue(m_destURI);
			}

			// Create a MessageProducer from the session to the topic or queue
			m_producer = m_session->createProducer(m_destination);
			m_producer->setDeliveryMode(DeliveryMode::NON_PERSISTENT);
		}
		catch(CMSException& e)
		{
			e.printStackTrace();
		}
	}

	void flush()
	{
		if(m_destination != NULL)
		{
			delete m_destination;
		}

		// Create the destination
		if(m_useTopic)
		{
			m_destination = m_session->createTopic(m_destURI);
		}
		else
		{
			m_destination = m_session->createQueue(m_destURI);
		}
	}

private:
	// Disable copy constructor and operator= overload
	ActiveMQProducer(const ActiveMQProducer&);
	ActiveMQProducer& operator=(const ActiveMQProducer&);

	// Member variables
	Connection *m_connection;
	Session *m_session;
	Destination *m_destination;
	MessageProducer *m_producer;
	bool m_useTopic;
	bool m_clientAck;
	std::string m_brokerURI;
	std::string m_destURI;

	// Clean up, clean up, everybody do your share
	void CleanUp()
	{
		// Destroy resources
		try
		{
			if(m_destination != NULL) delete m_destination;
		}
		catch(cms::CMSException& e)
		{
			e.printStackTrace();
		}
		m_destination = NULL;

		try
		{
			if(m_producer != NULL) delete m_producer;
		}
		catch(cms::CMSException& e)
		{
			e.printStackTrace();
		}
		m_producer = NULL;

		// Close open resources
		try
		{
			if(m_session != NULL) m_session->close();
			if(m_connection != NULL) m_connection->close();
		}
		catch(cms::CMSException& e)
		{
			e.printStackTrace();
		}

		try
		{
			if(m_session != NULL) delete m_session;
		}
		catch(cms::CMSException& e)
		{
			e.printStackTrace();
		}
		m_session = NULL;

		try
		{
			if(m_connection != NULL) delete m_connection;
		}
		catch(cms::CMSException& e)
		{
			e.printStackTrace();
		}
		m_connection = NULL;
	}
};