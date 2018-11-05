// Copyright 2016 Proyectos y Sistemas de Mantenimiento SL (eProsima).
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * @file HelloWorldSubscriber.cpp
 *
 */

#include "HelloWorldSubscriber.h"
#include <fastrtps/attributes/ParticipantAttributes.h>
#include <fastrtps/attributes/SubscriberAttributes.h>
#include <fastrtps/subscriber/Subscriber.h>
#include <fastrtps/Domain.h>
#include <fastrtps/utils/eClock.h>
#include <fastrtps/types/DynamicTypeBuilderFactory.h>
#include <fastrtps/types/DynamicDataFactory.h>
#include <fastrtps/types/DynamicTypeBuilder.h>
#include <fastrtps/types/DynamicTypeBuilderPtr.h>
#include <fastrtps/types/TypeDescriptor.h>
#include <fastrtps/types/MemberDescriptor.h>
#include <fastrtps/types/DynamicType.h>

using namespace eprosima::fastrtps;
using namespace eprosima::fastrtps::rtps;

HelloWorldSubscriber::HelloWorldSubscriber()
    : mp_participant(nullptr)
    , mp_subscriber(nullptr)
    , m_DynType(nullptr)
{
}

bool HelloWorldSubscriber::init()
{
    ParticipantAttributes PParam;
    PParam.rtps.builtin.domainId = 0;
    PParam.rtps.setName("DynHelloWorld_sub");
    mp_participant = Domain::createParticipant(PParam, (ParticipantListener*)&m_part_list);
    if(mp_participant==nullptr)
        return false;

    //  Create basic types and add members to the struct.
    DynamicTypeBuilder_ptr created_type_ulong = DynamicTypeBuilderFactory::GetInstance()->CreateUint32Builder();
    DynamicTypeBuilder_ptr created_type_string = DynamicTypeBuilderFactory::GetInstance()->CreateStringBuilder();
    DynamicTypeBuilder_ptr struct_type_builder = DynamicTypeBuilderFactory::GetInstance()->CreateStructBuilder();
    struct_type_builder->AddMember(0, "index", created_type_ulong.get());
    struct_type_builder->AddMember(1, "message", created_type_string.get());
    struct_type_builder->SetName("HelloWorld");
    DynamicType_ptr dynType = struct_type_builder->Build();
    m_DynType.SetDynamicType(dynType);
    m_listener.m_DynHello = DynamicDataFactory::GetInstance()->CreateData(dynType);

    //REGISTER THE TYPE
    Domain::registerDynamicType(mp_participant, &m_DynType);

    //CREATE THE SUBSCRIBER
    SubscriberAttributes Rparam;
    Rparam.topic.topicKind = NO_KEY;
    Rparam.topic.topicDataType = "HelloWorld";
    Rparam.topic.topicName = "HelloWorldTopic";
    //Rparam.topic.topicDiscoveryKind = NO_CHECK; // Do it compatible with other HelloWorlds

    mp_subscriber = Domain::createSubscriber(mp_participant,Rparam,(SubscriberListener*)&m_listener);

    if(mp_subscriber == nullptr)
        return false;


    return true;
}

HelloWorldSubscriber::~HelloWorldSubscriber() {
    // TODO Auto-generated destructor stub
    Domain::removeParticipant(mp_participant);

    DynamicDataFactory::GetInstance()->DeleteData(m_listener.m_DynHello);

    Domain::stopAll();
}

void HelloWorldSubscriber::SubListener::onSubscriptionMatched(Subscriber* /*sub*/,MatchingInfo& info)
{
    if(info.status == MATCHED_MATCHING)
    {
        n_matched++;
        std::cout << "Subscriber matched"<<std::endl;
    }
    else
    {
        n_matched--;
        std::cout << "Subscriber unmatched"<<std::endl;
    }
}

void HelloWorldSubscriber::PartListener::onParticipantDiscovery(Participant*, ParticipantDiscoveryInfo info)
{
    if (info.rtps.m_status == DISCOVERED_RTPSPARTICIPANT)
    {
        std::cout << "Participant " << info.rtps.m_RTPSParticipantName << " discovered" << std::endl;
    }
    else if (info.rtps.m_status == REMOVED_RTPSPARTICIPANT)
    {
        std::cout << "Participant removed" << std::endl;
    }
    else if (info.rtps.m_status == DROPPED_RTPSPARTICIPANT)
    {
        std::cout << "Participant " << info.rtps.m_RTPSParticipantName << " dropped" << std::endl;
    }
}

void HelloWorldSubscriber::SubListener::onNewDataMessage(Subscriber* sub)
{
    if(sub->takeNextData((void*)m_DynHello, &m_info))
    {
        if(m_info.sampleKind == ALIVE)
        {
            this->n_samples++;
            // Print your structure data here.
            std::string message;
            m_DynHello->GetStringValue(message, 1);
            uint32_t index;
            m_DynHello->GetUint32Value(index, 0);

            std::cout << "Message: "<<message<< " with index: "<<index<< " RECEIVED"<<std::endl;
        }
    }
}


void HelloWorldSubscriber::run()
{
    std::cout << "Subscriber running. Please press enter to stop the Subscriber" << std::endl;
    std::cin.ignore();
}

void HelloWorldSubscriber::run(uint32_t number)
{
    std::cout << "Subscriber running until "<< number << "samples have been received"<<std::endl;
    while(number > this->m_listener.n_samples)
        eClock::my_sleep(500);
}