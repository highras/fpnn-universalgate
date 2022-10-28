# Universal Gate

## 一、概述

1. 用途

	供支持FPNN／HTTP协议的任何语言开发的程序，以

	* 一致性哈希
	* 随机
	* 轮转
	* 强一致性/广播（Answer为达成成功条件后，任一服务实例的成功返回）
		* 所有成功
		* 任一成功
		* 过半成功
		* 指定数目成功

	的方式，访问FPNN生态系统内部注册到FPZK服务的任何服务。

1. 工作模式

	分为限定模式和动态模式。

	* 限定模式：

		只能访问被允许的服务。对于被允许的服务，只能以允许的路由方式访问。
	
	* 动态模式：
	
		只要在 FPZK 中注册的服务均可访问。只要是支持的路由方式，均可使用。
 

## 二、协议

本服务无特定协议，为目标服务的目标接口协议加上以下几个成员：

注：为了避免和目标服务的参数冲突，Universal Proxy Gate 的以下成员名称均以“$”开头。

| 成员 | 类型 | 说明 | 备注 |
|-----|------|-----|-----|
| $serverName | %s | 目标服务在FPZK中的注册名称 | 必须包含该成员 |
| $routingKind | %s | 路由方式。枚举值：carp (一致性哈希), consistency (强一致性／广播), random, rotatory | 必须包含该成员 |
| $hintId | %d | 如果 $routingKind == carp，指定一致性哈希所用的整数类型的 hint。 | 根据情况可选  如果与 $hintString 同时出现，$hintString 被忽略 |
| $hintString | %s | 如果 $routingKind == carp，指定一致性哈希所用字符串类型的 hint。 | 根据情况可选  如果与 $hintId 同时出现，$hintString 被忽略 |
| $consistencyCondition | %s | 如果 $routingKind == consistency，指定强一致性／广播的方式。  枚举值：all, anyOne, moreThanHalf, count | 根据情况可选 |
| $consistencyCount | %d | 如果 $routingKind == consistency，而且 $consistencyCondition == count，指定 count 的数量。 | 根据情况可选 |
| $method | %s | 如果不为空，则访问目标服务器上，该参数指定的接口。否则访问 quest 里原始指定的接口。 | 可选成员  主要用于访问FPNN框架内置接口。 |
| $timeout | %d | 超时控制。单位：秒 | 可选成员 |
 

## 三、配置

* 共同配置：

	除 FPNN 标准配置外，增加：

		FPZK.client.serverList =
		FPZK.client.projectName =
		FPZK.client.projectToken =
		FPZK.client.serviceName =
		FPZK.client.version =

* 动态模式：

	除 共同配置外，增加：

		UniversalProxyGate.dynamicProxies = true

* 限定模式：

	除 共同配置外，增加：

		UniversalProxyGate.dynamicProxies = false
		UniversalProxyGate.carp.names =
		UniversalProxyGate.consistency.names =
		UniversalProxyGate.random.names =
		UniversalProxyGate.rotatory.names =
		# names 为在FPZK 中注册的服务名称列表，用逗号分隔。

 

## 四、例外

1. 对于以 * 号开头的 FPNN 框架内置接口的访问：

	对于 *tune、*infos 等 FPNN 框架内置接口，务必使用 $method 参数指定目标接口，且：

	1. 如果为 FPNN 协议访问，quest 中的method 中请任意填写，但不能以 * 号开始；
	1. 如果为 HTTP 协议，接口名请任意填写，但不能以 * 号开始；

	否则 * 号接口会被 UniversalProxyGate 自动接收处理，不会转发到目标服务器。