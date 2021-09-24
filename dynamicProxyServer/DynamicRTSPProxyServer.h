//
// Created by ubuntu on 2021/9/1.
//

#ifndef LIVE_DYNAMICRTSPPROXYSERVER_H
#define LIVE_DYNAMICRTSPPROXYSERVER_H


#include "RTSPServer.hh"

class DynamicRTSPProxyServer : public RTSPServer {
public:
    static DynamicRTSPProxyServer* createNew(UsageEnvironment& env, Port ourPort = 554,
                                 UserAuthenticationDatabase* authDatabase = NULL,
                                 unsigned reclamationSeconds = 65);
protected:
    DynamicRTSPProxyServer(UsageEnvironment& env, int ourSocketIPv4, int ourSocketIPv6, Port ourPort,
    UserAuthenticationDatabase* authDatabase, unsigned reclamationTestSeconds);
    // called only by createNew();
    virtual ~DynamicRTSPProxyServer();

protected: // redefined virtual functions
    virtual void lookupServerMediaSession(char const* streamName,
                                          lookupServerMediaSessionCompletionFunc* completionFunc,
                                          void* completionClientData,
                                          Boolean isFirstLookupInSession);
};


#endif //LIVE_DYNAMICRTSPPROXYSERVER_H
