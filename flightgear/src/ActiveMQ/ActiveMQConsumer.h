#pragma once

#include <activemq/library/ActiveMQCPP.h>
#include <decaf/lang/Thread.h>
#include <decaf/lang/Runnable.h>
#include <decaf/util/concurrent/CountDownLatch.h>
#include <decaf/lang/Integer.h>
#include <decaf/lang/Long.h>
#include <decaf/lang/System.h>
#include <activemq/core/ActiveMQConnectionFactory.h>
#include <activemq/util/Config.h>
#include <cms/Connection.h>
#include <cms/Session.h>
#include <cms/TextMessage.h>
#include <cms/ExceptionListener.h>
#include <cms/MessageListener.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <memory>

using namespace activemq::core;
using namespace decaf::util::concurrent;
using namespace decaf::util;
using namespace decaf::lang;
using namespace cms;
using namespace std;

class ActiveMQConsumer : public ExceptionListener,
	public MessageListener,
	public Runnable 
{
public:
	/*! Constructor
	 * @param brokerURI
	 * @param useTopic
	 * @param sessionTransacted
	 * @param waitMillis
	 */
	ActiveMQConsumer(const string &brokerURI, bool useTopic = false, bool sessionTransacted = false,
		int waitMillis = 30000) : 
	m_latch(1),
	m_doneLatch(1000)
	{
		m_connection = NULL;
		m_session = NULL;
		m_destination = NULL;
		m_consumer = NULL;
		m_waitMillis = waitMillis;
		m_useTopic = useTopic;
		m_sessionTransacted = sessionTransacted;
		m_brokerURI = brokerURI;

		m_producerSubsystem = NULL;
	}

	/*! Destructor
	*/
	virtual ~ActiveMQConsumer() {cleanup();}

	void close() {cleanup();}

	void waitUntilReady() {m_latch.await();}

	/*! Initialize connection to receive messages
	*/
	virtual void run()
	{
		try
		{
			// Create a ConnectionFactory
			auto_ptr<ConnectionFactory> connectionFactory(
				ConnectionFactory::createCMSConnectionFactory(m_brokerURI));

			// Create a Connection
			m_connection = connectionFactory->createConnection();
			m_connection->start();
			m_connection->setExceptionListener(this);

			// Create a Session
			if(m_sessionTransacted)
			{
				m_session = m_connection->createSession(Session::SESSION_TRANSACTED);
			}
			else
			{
				m_session = m_connection->createSession(Session::AUTO_ACKNOWLEDGE);
			}

			// Create the destination (Topic or Queue)
			if(m_useTopic)
			{
				m_destination = m_session->createTopic("MESSAGE_INTERVAL");
			}
			else
			{
				m_destination = m_session->createQueue("MESSAGE_INTERVAL");
			}

			// Create a MessageConsumer from the Session to the Topic or Queue
			m_consumer = m_session->createConsumer(m_destination);

			m_consumer->setMessageListener(this);

			cout.flush();
			cerr.flush();

			// Indicate we are ready for messages
			m_latch.countDown();

			// Wait while asynchronous messages come in
			//m_doneLatch.await(m_waitMillis);
		}
		catch(CMSException& e)
		{
			// Indicate we are ready for messages
			m_latch.countDown();
			e.printStackTrace();
		}
	}

	// Called from the consumer since this class is a registered MessageListener
	virtual void onMessage(const Message *message)
	{
		static int count = 0;

		try
		{
			count++;
			const TextMessage *textMessage = dynamic_cast<const TextMessage*> (message);
			string text = "";

			if(textMessage != NULL)
			{
				text = textMessage->getText();
				if(m_producerSubsystem != NULL)
				{
					double interval = atof(text.c_str());
					m_producerSubsystem->setMessageRate(interval * 1000);
				}
			}
			else
			{
				text = "NOT A TEXTMESSAGE!";
			}
			printf("Message #%d Received: %s\n", count, text.c_str());
		}
		catch(CMSException &e)
		{
			e.printStackTrace();
		}

		// Commit all message
		if(m_sessionTransacted)
		{
			m_session->commit();
		}

		// No matter what, tag the count down latch until done
		//m_doneLatch.countDown();
	}

	void setProducerSubsystem(ActiveMQProducerSubsystem *ps) {m_producerSubsystem = ps;}

	// If something bad happens you see it here as this class is also been
	// registered as an ExceptionListener with the connection.
	virtual void onException(const CMSException& ex AMQCPP_UNUSED)
	{
		printf("CMS Exception occurred. Shutting down client.\n");
		ex.printStackTrace();
		exit(1);
	}

private:
	CountDownLatch	m_latch;
	CountDownLatch	m_doneLatch;
	Connection		*m_connection;
	Session			*m_session;
	Destination		*m_destination;
	MessageConsumer	*m_consumer;
	long			m_waitMillis;
	bool			m_useTopic;
	bool			m_sessionTransacted;
	string			m_brokerURI;

	//! Producer subystem so we can change the message rate
	ActiveMQProducerSubsystem *m_producerSubsystem;

	// Disable copy constructor and operator = overload
	ActiveMQConsumer(const ActiveMQConsumer&);
	ActiveMQConsumer& operator=(const ActiveMQConsumer&);

	void cleanup()
	{
		if(m_connection != NULL)
		{
			try
			{
				m_connection->close();
			}
			catch(CMSException& ex)
			{
				ex.printStackTrace();
			}

			// Destroy resources
			try
			{
				if(m_destination != NULL) {delete m_destination;}
				m_destination = NULL;
				if(m_consumer != NULL) {delete m_consumer;}
				m_consumer = NULL;
				if(m_session != NULL) {delete m_session;}
				m_session = NULL;
				if(m_connection != NULL) {delete m_connection;}
			}
			catch(CMSException &ex)
			{
				ex.printStackTrace();
			}
		}
	}
};