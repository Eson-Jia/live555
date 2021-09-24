//
// Created by ubuntu on 2021/9/3.
//

#ifndef LIVE_ASYNCPROXYSERVERMEDIASESSION_H
#define LIVE_ASYNCPROXYSERVERMEDIASESSION_H

#include "ProxyServerMediaSession.hh"
#include "OnDemandServerMediaSubsession.hh"
#include "playCommon.hh"

class AsyncProxyRTSPClient : public ProxyRTSPClient {
public:
    AsyncProxyRTSPClient(class ProxyServerMediaSession &ourServerMediaSession, char const *rtspURL,
                         char const *username, char const *password,
                         portNumBits tunnelOverHTTPPortNum, int verbosityLevel, int socketNumToServer);

    ~AsyncProxyRTSPClient();
    void continueAfterDESCRIBE(char const* sdpDescription) override;
private:
    void sendDESCRIBE();

    friend class AsyncProxyServerMediaSession;

    friend class AsyncProxyServerMediaSubsession;

    Boolean fDoneDESCRIBE{};
    Authenticator *fOurAuthenticator;

    Authenticator *auth() { return fOurAuthenticator; }
};

class AsyncProxyServerMediaSession;

typedef AsyncProxyRTSPClient *
        createNewAsyncProxyRTSPClientFunc(AsyncProxyServerMediaSession &ourServerMediaSession,
                                          char const *rtspURL,
                                          char const *username, char const *password,
                                          portNumBits tunnelOverHTTPPortNum, int verbosityLevel,
                                          int socketNumToServer);

AsyncProxyRTSPClient *
defaultCreateNewAsyncProxyRTSPClientFunc(AsyncProxyServerMediaSession &ourServerMediaSession,
                                         char const *rtspURL,
                                         char const *username, char const *password,
                                         portNumBits tunnelOverHTTPPortNum, int verbosityLevel,
                                         int socketNumToServer);

class AsyncProxyServerMediaSubsession : public OnDemandServerMediaSubsession {
public:
    AsyncProxyServerMediaSubsession(MediaSubsession &mediaSubsession,
                                    portNumBits initialPortNum, Boolean multiplexRTCPWithRTP);

    ~AsyncProxyServerMediaSubsession();

    char const *codecName() const { return fCodecName; }

    char const *url() const;

private: // redefined virtual functions
    virtual FramedSource *createNewStreamSource(unsigned clientSessionId,
                                                unsigned &estBitrate);

    virtual void closeStreamSource(FramedSource *inputSource);

    virtual RTPSink *createNewRTPSink(Groupsock *rtpGroupsock,
                                      unsigned char rtpPayloadTypeIfDynamic,
                                      FramedSource *inputSource);

    virtual Groupsock *createGroupsock(struct sockaddr_storage const &addr, Port port);

    virtual RTCPInstance *createRTCP(Groupsock *RTCPgs, unsigned totSessionBW, /* in kbps */
                                     unsigned char const *cname, RTPSink *sink);

private:
    static void subsessionByeHandler(void *clientData);

    void subsessionByeHandler();

    int verbosityLevel() const;

private:
    friend class ProxyRTSPClient;

    MediaSubsession &fClientMediaSubsession; // the 'client' media subsession object that corresponds to this 'server' media subsession
    char const *fCodecName;  // copied from "fClientMediaSubsession" once it's been set up
    AsyncProxyServerMediaSubsession *fNext; // used when we're part of a queue
    Boolean fHaveSetupStream;
};


class AsyncProxyServerMediaSession : public ProxyServerMediaSession {
public:
    static AsyncProxyServerMediaSession *createNew(UsageEnvironment &env,
                                                   GenericMediaServer *ourMediaServer, // Note: We can be used by just one server
                                                   char const *inputStreamURL, // the "rtsp://" URL of the stream we'll be proxying
                                                   char const *streamName = NULL,
                                                   char const *username = NULL,
                                                   char const *password = NULL,
                                                   Boolean *finishedDescribe = NULL,
                                                   portNumBits tunnelOverHTTPPortNum = 0,
                                                   int verbosityLevel = 0,
                                                   int socketNumToServer = -1,
                                                   MediaTranscodingTable *transcodingTable = NULL);

    ~AsyncProxyServerMediaSession();

protected:
    AsyncProxyServerMediaSession(UsageEnvironment
                                 &env,
                                 GenericMediaServer *ourMediaServer,
                                 char const *inputStreamURL,
                                 char const *streamName,
                                 char const *username,
                                 char const *password,
                                 portNumBits
                                 tunnelOverHTTPPortNum,
                                 int verbosityLevel,
                                 int socketNumToServer,
                                 MediaTranscodingTable *transcodingTable,
                                 Boolean *finishedDescribe,
                                 createNewAsyncProxyRTSPClientFunc *ourCreateNewAsyncProxyRTSPClientFunc
                                 = defaultCreateNewAsyncProxyRTSPClientFunc,
                                 portNumBits initialPortNum = 6970,
                                 Boolean
                                 multiplexRTCPWithRTP = False);


protected:
    friend class AsyncProxyRTSPClient;

    friend class AsyncProxyServerMediaSubsession;

    virtual void continueAfterDESCRIBE(char const *sdpDescription);

    void resetDESCRIBEState(); // undoes what was done by "contineAfterDESCRIBE()"

private:
    int fVerbosityLevel;

    class PresentationTimeSessionNormalizer *fPresentationTimeSessionNormalizer;

    createNewAsyncProxyRTSPClientFunc *fCreateNewAsyncProxyRTSPClientFunc;
    MediaTranscodingTable *fTranscodingTable;
    portNumBits fInitialPortNum;
    Boolean fMultiplexRTCPWithRTP;
    Boolean *fFinishedDescribe;
    AsyncProxyRTSPClient *fProxyRTSPClient;
};

static void continueAfterDESCRIBE(RTSPClient *rtspClient, int resultCode, char *resultString) {
    char const *res;

    if (resultCode == 0) {
        // The "DESCRIBE" command succeeded, so "resultString" should be the stream's SDP description.
        res = resultString;
    } else {
        // The "DESCRIBE" command failed.
        res = NULL;
    }
    ((AsyncProxyRTSPClient *) rtspClient)->continueAfterDESCRIBE(res);
    delete[] resultString;
}


#endif //LIVE_ASYNCPROXYSERVERMEDIASESSION_H
