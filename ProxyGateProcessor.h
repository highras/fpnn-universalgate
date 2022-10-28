#ifndef Proxy_Gate_Processor_h
#define Proxy_Gate_Processor_h

#include "LruHashMap.h"
#include "IQuestProcessor.h"
#include "FPZKClient.h"
#include "TCPFPZKCarpProxy.hpp"
#include "TCPFPZKConsistencyProxy.hpp"
#include "TCPFPZKRandomProxy.hpp"
#include "TCPFPZKRotatoryProxy.hpp"

using namespace fpnn;

class ProxyGateProcessor: public IQuestProcessor
{
	QuestProcessorClassPrivateFields(ProxyGateProcessor)
	
	bool _dynamicProxies;

	std::mutex _mutex;
	FPZKClientPtr _fpzk;
	LruHashMap<std::string, TCPFPZKCarpProxyPtr> _carpProxies;
	LruHashMap<std::string, TCPFPZKConsistencyProxyPtr> _consistencyProxies;
	LruHashMap<std::string, TCPFPZKRandomProxyPtr> _randomProxies;
	LruHashMap<std::string, TCPFPZKRotatoryProxyPtr> _rotatoryProxies;

	FPAnswerPtr processCarpProxy(const std::string& method_name, const FPReaderPtr args, const FPQuestPtr quest, const ConnectionInfo& connInfo);
	FPAnswerPtr processConsistencyProxy(const std::string& method_name, const FPReaderPtr args, const FPQuestPtr quest, const ConnectionInfo& connInfo);
	FPAnswerPtr processRandomProxy(const std::string& method_name, const FPReaderPtr args, const FPQuestPtr quest, const ConnectionInfo& connInfo);
	FPAnswerPtr processRotatoryProxy(const std::string& method_name, const FPReaderPtr args, const FPQuestPtr quest, const ConnectionInfo& connInfo);

public:
	virtual FPAnswerPtr unknownMethod(const std::string& method_name, const FPReaderPtr args, const FPQuestPtr quest, const ConnectionInfo& connInfo);
	ProxyGateProcessor();

	QuestProcessorClassBasicPublicFuncs
};

#endif