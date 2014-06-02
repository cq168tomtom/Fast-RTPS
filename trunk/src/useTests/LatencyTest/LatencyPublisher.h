/*************************************************************************
 * Copyright (c) 2014 eProsima. All rights reserved.
 *
 * This copy of eProsima RTPS is licensed to you under the terms described in the
 * EPROSIMARTPS_LIBRARY_LICENSE file included in this distribution.
 *
 *************************************************************************/

/**
 * @file LatencyPublisher.h
 *
 *  Created on: Jun 2, 2014
 *      Author: Gonzalo Rodriguez Canosa
 *      email:  gonzalorodriguez@eprosima.com
 *              grcanosa@gmail.com  	
 */

#ifndef LATENCYPUBLISHER_H_
#define LATENCYPUBLISHER_H_

#include <numeric>
#include <cmath>

#include "eprosimartps/rtps_all.h"
#include "eprosimartps/utils/eClock.h"

struct TimeStats{
	uint32_t nbytes;
	uint64_t min,max,mean,stdev,p50,p90,p99,p9999;
};

long toNanoSec(Time_t& t)
{
	return t.nanoseconds + t.seconds*pow(2,32);
}

class LatencyPublisher : public SubscriberListener
{
public:
	LatencyPublisher();
	~LatencyPublisher(){};
	Participant* m_part;
	Publisher* m_pub;
	Subscriber* m_sub;
	LatencyType* m_latency_out;
	LatencyType* m_latency_in;
	SampleInfo_t m_info;
	Time_t m_t1;
	Time_t m_t2;
	eClock clock;
	long overhead_value;
	std::vector<uint64_t> m_times;
	std::vector<TimeStats> m_stats;
	boost::interprocess::interprocess_semaphore sema;
	bool test(uint32_t datasize,uint32_t n_samples);
	void analyzeTimes(uint32_t datasize);
	void printStat(TimeStats& TS);

	void onNewDataMessage()
	{
		m_sub->readNextData((void*)m_latency_in,&m_info);
		clock.setTimeNow(&m_t2);
		m_times.push_back(toNanoSec(m_t2)-toNanoSec(m_t1)-overhead_value);
	}
	void onSubscriptionMatched()
	{
		cout << B_RED << "SUBSCRIPTION MATCHED" <<DEF << endl;
		sema.post();
	}
};



LatencyPublisher::LatencyPublisher():
		m_latency_out(NULL),m_latency_in(NULL),sema(0)
{
	ParticipantAttributes PParam;
	PParam.defaultSendPort = 10042;
	PParam.discovery.use_SIMPLE_EndpointDiscoveryProtocol = true;
	PParam.discovery.use_SIMPLE_ParticipantDiscoveryProtocol = true;
	PParam.discovery.m_simpleEDP.use_PublicationReaderANDSubscriptionWriter = true;
	PParam.discovery.m_simpleEDP.use_PublicationWriterANDSubscriptionReader = true;
	PParam.name = "participant1";
	m_part = DomainParticipant::createParticipant(PParam);

	clock.setTimeNow(&m_t1);
	for(int i=0;i<400;i++)
		clock.setTimeNow(&m_t2);
	overhead_value = (toNanoSec(m_t2)-toNanoSec(m_t1))/400;
	cout << "Overhead " << overhead_value << endl;
	//PUBLISHER
	PublisherAttributes Wparam;
	Wparam.historyMaxSize = 1100;
	Wparam.topic.topicDataType = "LatencyType";
	Wparam.topic.topicKind = NO_KEY;
	Wparam.topic.topicName = "LatencyUp";
	m_pub = DomainParticipant::createPublisher(m_part,Wparam);
	//SUBSCRIBER
	SubscriberAttributes Rparam;
	Rparam.historyMaxSize = 1100;
	Rparam.topic.topicDataType = std::string("LatencyType");
	Rparam.topic.topicKind = NO_KEY;
	Rparam.topic.topicName = "LatencyDown";
	m_sub = DomainParticipant::createSubscriber(m_part,Rparam);
	m_sub->assignListener((SubscriberListener*)this);

}

bool LatencyPublisher::test(uint32_t datasize,uint32_t n_samples)
{
	m_latency_in = new LatencyType(datasize);
	m_latency_out = new LatencyType(datasize);
	m_times.clear();

	for(uint32_t i =0;i<n_samples;++i)
	{
		m_latency_out->seqnum++;
		clock.setTimeNow(&m_t1);
		m_pub->write((void*)m_latency_out);
		m_sub->waitForUnreadMessage();
		if(!(*m_latency_out == *m_latency_in))
		{
			cout << "PROBLEM"<<endl;
			return false;
		}
	}
	int32_t removed=0;
	m_pub->removeAllChange(&removed);
	for(uint8_t i =0;i<m_sub->getHistoryElementsNumber();++i)
		m_sub->takeNextData((void*)m_latency_in,&m_info);

	analyzeTimes(datasize);
	printStat(*(m_stats.end()-1));
	delete(m_latency_in);
	delete(m_latency_out);
	return true;
}



void LatencyPublisher::analyzeTimes(uint32_t datasize)
{
	TimeStats TS;
	TS.min = *std::min_element(m_times.begin(),m_times.end());
	TS.max = *std::max_element(m_times.begin(),m_times.end());
	TS.mean = std::accumulate(m_times.begin(),m_times.end(),0)/m_times.size();
	TS.stdev = 0;
	for(std::vector<uint64_t>::iterator tit=m_times.begin();tit!=m_times.end();++tit)
	{
		TS.stdev += pow(*tit-TS.mean,2);
	}
	TS.stdev = sqrt(TS.stdev/m_times.size());
	std::sort(m_times.begin(),m_times.end());
	double x= 0;
	double elem,dec;
	x = m_times.size()*0.5;
	dec = modf(x,&elem);
	if(dec == 0)
		TS.p50 = (m_times.at(elem)+m_times.at(elem+1))/2;
	else
		TS.p50 = m_times.at(elem+1);
	x = m_times.size()*0.9;
	dec = modf(x,&elem);
	if(dec == 0)
		TS.p90 = (m_times.at(elem-1)+m_times.at(elem))/2;
	else
		TS.p90 = m_times.at(elem);
	x = m_times.size()*0.99;
	dec = modf(x,&elem);
	if(dec == 0)
		TS.p99 = (m_times.at(elem-1)+m_times.at(elem))/2;
	else
		TS.p99 = m_times.at(elem);
	x = m_times.size()*0.9999;
	dec = modf(x,&elem);
	if(dec == 0)
		TS.p9999 = (m_times.at(elem-1)+m_times.at(elem))/2;
	else
		TS.p9999 = m_times.at(elem);

	m_stats.push_back(TS);

}

void LatencyPublisher::printStat(TimeStats& TS)
{
	printf("%6u,%6lu,%6lu,%6lu,%6lu,%6lu,%6lu,%6lu,%6lu,%6lu\n",TS.nbytes,TS.mean,TS.stdev,TS.min,TS.max,
													TS.p50,TS.p90,TS.p99,TS.p9999);
}



#endif /* LATENCYPUBLISHER_H_ */