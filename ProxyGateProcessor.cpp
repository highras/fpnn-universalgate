#include "FPLog.h"
#include "Setting.h"
#include "StringUtil.h"
#include "ProxyGateProcessor.h"

namespace ErrorInfo
{
	const int errorBase = 10000 * 10;

	const int InvalidRoutingKindCode = errorBase + 1;
	const int InvalidServiceKindCode = errorBase + 2;
	const int InvalidConsistencyConditionCode = errorBase + 3;

	const char* const raiser_UPG = "UniversalGate";
}
const size_t SLOT_NUM = 64;
ProxyGateProcessor::ProxyGateProcessor(): _carpProxies(SLOT_NUM), _consistencyProxies(SLOT_NUM),
	_randomProxies(SLOT_NUM), _rotatoryProxies(SLOT_NUM)
{
	_dynamicProxies = Setting::getBool("UniversalGate.dynamicProxies", false);

	std::string serverList = Setting::getString("FPZK.client.serverList");
	std::string projectName = Setting::getString("FPZK.client.projectName");
	std::string projectToken = Setting::getString("FPZK.client.projectToken");
	std::string serviceName = Setting::getString("FPZK.client.serviceName");
	std::string version = Setting::getString("FPZK.client.version");

	_fpzk = FPZKClient::create(serverList, projectName, projectToken);
	_fpzk->registerService(serviceName, version);

	if (_dynamicProxies)
		return;

	//-- Carp Proxies
	{
		std::string namelist = Setting::getString("UniversalGate.carp.names");
		std::vector<std::string> names;
		StringUtil::split(namelist, ",; ", names);

		for (std::string& name: names)
		{
			TCPFPZKCarpProxyPtr proxy(new TCPFPZKCarpProxy(_fpzk, name));
			_carpProxies.insert(name, proxy);
		}
	}
	//-- Consistency Proxies
	{
		std::string namelist = Setting::getString("UniversalGate.consistency.names");
		std::vector<std::string> names;
		StringUtil::split(namelist, ",; ", names);

		for (std::string& name: names)
		{
			TCPFPZKConsistencyProxyPtr proxy(new TCPFPZKConsistencyProxy(_fpzk, name, ConsistencySuccessCondition::AllQuestsSuccess, 0));
			_consistencyProxies.insert(name, proxy);
		}
	}
	//-- Random Proxies
	{
		std::string namelist = Setting::getString("UniversalGate.random.names");
		std::vector<std::string> names;
		StringUtil::split(namelist, ",; ", names);

		for (std::string& name: names)
		{
			TCPFPZKRandomProxyPtr proxy(new TCPFPZKRandomProxy(_fpzk, name));
			_randomProxies.insert(name, proxy);
		}
	}
	//-- Rotatory Proxies
	{
		std::string namelist = Setting::getString("UniversalGate.rotatory.names");
		std::vector<std::string> names;
		StringUtil::split(namelist, ",; ", names);

		for (std::string& name: names)
		{
			TCPFPZKRotatoryProxyPtr proxy(new TCPFPZKRotatoryProxy(_fpzk, name));
			_rotatoryProxies.insert(name, proxy);
		}
	}
}

FPAnswerPtr ProxyGateProcessor::unknownMethod(const std::string& method_name, const FPReaderPtr args, const FPQuestPtr quest, const ConnectionInfo& connInfo)
{
	std::string routingKind = args->wantString("$routingKind");
	std::string method = args->getString("$method");
	if (method.empty())
		method = method_name;

	if (routingKind == "carp")
	{
		return processCarpProxy(method, args, quest, connInfo);
	}
	else if (routingKind == "consistency")
	{
		return processConsistencyProxy(method, args, quest, connInfo);
	}
	else if (routingKind == "random")
	{
		return processRandomProxy(method, args, quest, connInfo);
	}
	else if (routingKind == "rotatory")
	{
		return processRotatoryProxy(method, args, quest, connInfo);
	}
	else
	{
		return FPAWriter::errorAnswer(quest, ErrorInfo::InvalidRoutingKindCode, "Invalid routing kind.", ErrorInfo::raiser_UPG);
	}
}

class TargetAnswerCallback: public AnswerCallback
{
	IAsyncAnswerPtr _async;

public:
	TargetAnswerCallback(IAsyncAnswerPtr async): _async(async) {}

	virtual void onAnswer(FPAnswerPtr answer)
	{
		//std::string jsonBody = answer->json();
		//FPAWriter aw(jsonBody, _async->getQuest());
		//_async->sendAnswer(aw.take());
		_async->sendAnswer(FPAWriter::CloneAnswer(answer, _async->getQuest()));
	}

	virtual void onException(FPAnswerPtr answer, int errorCode)
	{
		if (answer != nullptr)
		{
			FPAReader ar(answer);
			FPAnswerPtr an = FPAWriter::errorAnswer(_async->getQuest(), ar.getInt("code"), ar.getString("ex"), ar.getString("raiser"));
			_async->sendAnswer(an);
		}
		else
		{
			FPAnswerPtr an = FPAWriter::errorAnswer(_async->getQuest(), errorCode, "N/A", ErrorInfo::raiser_UPG);
			_async->sendAnswer(an);
		}
	}
};

FPAnswerPtr ProxyGateProcessor::processCarpProxy(const std::string& method_name, const FPReaderPtr args, const FPQuestPtr quest, const ConnectionInfo& connInfo)
{
	std::string serverName = args->wantString("$serverName");
	int timeout = args->getInt("$timeout", 0);

	TCPFPZKCarpProxyPtr proxy;
	{
		std::lock_guard<std::mutex> lck (_mutex);
		LruHashMap<std::string, TCPFPZKCarpProxyPtr>::node_type* node = _carpProxies.use(serverName);
		if (node)
			proxy = node->data;
		else
		{
			if (_dynamicProxies)
			{
				proxy.reset(new TCPFPZKCarpProxy(_fpzk, serverName));
				_carpProxies.insert(serverName, proxy);
			}
			else
				return FPAWriter::errorAnswer(quest, ErrorInfo::InvalidServiceKindCode, "Invalid server name.", ErrorInfo::raiser_UPG);
		}
	}

	IAsyncAnswerPtr async = genAsyncAnswer(quest);
	//FPQWriter qw(method_name, quest->json());
	FPQuestPtr proxyQuest = FPQWriter::CloneQuest(method_name, quest);

	try
	{
		int64_t hintId = args->wantInt("$hintId");
		TargetAnswerCallback* cb = new TargetAnswerCallback(async);
		if (proxy->sendQuest(hintId, proxyQuest, cb, timeout) == false)
			delete cb;
	}
	catch (const FpnnError& ex)
	{
		std::string hintString = args->getString("$intIdString");  //-- 兼容之前错误的版本
		if (hintString.empty())
			hintString = args->wantString("$hintString");

		TargetAnswerCallback* cb = new TargetAnswerCallback(async);
		if (proxy->sendQuest(hintString, proxyQuest, cb, timeout) == false)
			delete cb;
	}

	return nullptr;
}

FPAnswerPtr ProxyGateProcessor::processConsistencyProxy(const std::string& method_name, const FPReaderPtr args, const FPQuestPtr quest, const ConnectionInfo& connInfo)
{
	int requiredCount = 0;
	ConsistencySuccessCondition condition;
	std::string consistencyCondition = args->wantString("$consistencyCondition");
	if (consistencyCondition == "all")
	{
		condition = ConsistencySuccessCondition::AllQuestsSuccess;
	}
	else if (consistencyCondition == "anyOne")
	{
		condition = ConsistencySuccessCondition::OneQuestSuccess;
	}
	else if (consistencyCondition == "moreThanHalf")
	{
		condition = ConsistencySuccessCondition::HalfQuestsSuccess;
	}
	else if (consistencyCondition == "count")
	{
		requiredCount = args->wantInt("$consistencyCondition");
		condition = ConsistencySuccessCondition::CountedQuestsSuccess;
	}
	else
		return FPAWriter::errorAnswer(quest, ErrorInfo::InvalidConsistencyConditionCode, "Invalid consistency condition.", ErrorInfo::raiser_UPG);


	std::string serverName = args->wantString("$serverName");
	int timeout = args->getInt("$timeout", 0);

	TCPFPZKConsistencyProxyPtr proxy;
	{
		std::lock_guard<std::mutex> lck (_mutex);
		LruHashMap<std::string, TCPFPZKConsistencyProxyPtr>::node_type* node = _consistencyProxies.use(serverName);
		if (node)
			proxy = node->data;
		else
		{
			if (_dynamicProxies)
			{
				proxy.reset(new TCPFPZKConsistencyProxy(_fpzk, serverName, condition, requiredCount));
				_consistencyProxies.insert(serverName, proxy);
			}
			else
				return FPAWriter::errorAnswer(quest, ErrorInfo::InvalidServiceKindCode, "Invalid server name.", ErrorInfo::raiser_UPG);
		}
	}

	IAsyncAnswerPtr async = genAsyncAnswer(quest);
	//FPQWriter qw(method_name, quest->json());
	FPQuestPtr proxyQuest = FPQWriter::CloneQuest(method_name, quest);

	TargetAnswerCallback* cb = new TargetAnswerCallback(async);
	if (proxy->sendQuest(proxyQuest, cb, condition, requiredCount, timeout) == false)
		delete cb;

	return nullptr;
}

FPAnswerPtr ProxyGateProcessor::processRandomProxy(const std::string& method_name, const FPReaderPtr args, const FPQuestPtr quest, const ConnectionInfo& connInfo)
{
	std::string serverName = args->wantString("$serverName");
	int timeout = args->getInt("$timeout", 0);

	TCPFPZKRandomProxyPtr proxy;
	{
		std::lock_guard<std::mutex> lck (_mutex);
		LruHashMap<std::string, TCPFPZKRandomProxyPtr>::node_type* node = _randomProxies.use(serverName);
		if (node)
			proxy = node->data;
		else
		{
			if (_dynamicProxies)
			{
				proxy.reset(new TCPFPZKRandomProxy(_fpzk, serverName));
				_randomProxies.insert(serverName, proxy);
			}
			else
				return FPAWriter::errorAnswer(quest, ErrorInfo::InvalidServiceKindCode, "Invalid server name.", ErrorInfo::raiser_UPG);
		}
	}

	IAsyncAnswerPtr async = genAsyncAnswer(quest);
	//FPQWriter qw(method_name, quest->json());
	FPQuestPtr proxyQuest = FPQWriter::CloneQuest(method_name, quest);

	TargetAnswerCallback* cb = new TargetAnswerCallback(async);
	if (proxy->sendQuest(proxyQuest, cb, timeout) == false)
		delete cb;

	return nullptr;
}

FPAnswerPtr ProxyGateProcessor::processRotatoryProxy(const std::string& method_name, const FPReaderPtr args, const FPQuestPtr quest, const ConnectionInfo& connInfo)
{
	std::string serverName = args->wantString("$serverName");
	int timeout = args->getInt("$timeout", 0);

	TCPFPZKRotatoryProxyPtr proxy;
	{
		std::lock_guard<std::mutex> lck (_mutex);
		LruHashMap<std::string, TCPFPZKRotatoryProxyPtr>::node_type* node = _rotatoryProxies.use(serverName);
		if (node)
			proxy = node->data;
		else
		{
			if (_dynamicProxies)
			{
				proxy.reset(new TCPFPZKRotatoryProxy(_fpzk, serverName));
				_rotatoryProxies.insert(serverName, proxy);
			}
			else
				return FPAWriter::errorAnswer(quest, ErrorInfo::InvalidServiceKindCode, "Invalid server name.", ErrorInfo::raiser_UPG);
		}
	}

	IAsyncAnswerPtr async = genAsyncAnswer(quest);
	//FPQWriter qw(method_name, quest->json());
	FPQuestPtr proxyQuest = FPQWriter::CloneQuest(method_name, quest);

	TargetAnswerCallback* cb = new TargetAnswerCallback(async);
	if (proxy->sendQuest(proxyQuest, cb, timeout) == false)
		delete cb;

	return nullptr;
}
