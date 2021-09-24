//
// Created by ubuntu on 2021/9/3.
//

#include <OnDemandServerMediaSubsession.hh>
#include "AsyncProxyServerMediaSession.h"

////////// AsyncProxyServerMediaSession implementation //////////

UsageEnvironment &operator<<(UsageEnvironment &env, const AsyncProxyServerMediaSession &psms) { // used for debugging
    return env << "ProxyServerMediaSession[" << psms.url() << "]";
}

AsyncProxyServerMediaSession::AsyncProxyServerMediaSession(
        UsageEnvironment
        &env,
        GenericMediaServer *ourMediaServer,
        char const *inputStreamURL,
        char const *streamName,
        char const *username,
        char const *password,
        portNumBits tunnelOverHTTPPortNum,
        int verbosityLevel,
        int socketNumToServer,
        MediaTranscodingTable *transcodingTable,
        Boolean *finishedDescribe,
        createNewAsyncProxyRTSPClientFunc *ourCreateNewAsyncProxyRTSPClientFunc,
        portNumBits initialPortNum,
        Boolean multiplexRTCPWithRTP) : ProxyServerMediaSession(
        env, ourMediaServer, inputStreamURL, streamName, username, password, tunnelOverHTTPPortNum, verbosityLevel,
        socketNumToServer,
        transcodingTable, defaultCreateNewProxyRTSPClientFunc, initialPortNum, multiplexRTCPWithRTP
),
                                        fCreateNewAsyncProxyRTSPClientFunc(ourCreateNewAsyncProxyRTSPClientFunc),
                                        fFinishedDescribe(finishedDescribe) {


    fProxyRTSPClient
            = (*fCreateNewAsyncProxyRTSPClientFunc)(*this, inputStreamURL, username, password,
                                                    tunnelOverHTTPPortNum,
                                                    verbosityLevel > 0 ? verbosityLevel - 1 : verbosityLevel,
                                                    socketNumToServer);
    fProxyRTSPClient->sendDESCRIBE();
}

AsyncProxyServerMediaSession::~AsyncProxyServerMediaSession() {

}

AsyncProxyServerMediaSession *
AsyncProxyServerMediaSession::createNew(UsageEnvironment &env, GenericMediaServer *ourMediaServer,
                                        char const *inputStreamURL, char const *streamName,
                                        char const *username, char const *password,
                                        Boolean *finishedDescribe, portNumBits tunnelOverHTTPPortNum,
                                        int verbosityLevel, int socketNumToServer,
                                        MediaTranscodingTable *transcodingTable) {
    return new AsyncProxyServerMediaSession(env, ourMediaServer, inputStreamURL, streamName, username, password,
                                            tunnelOverHTTPPortNum, verbosityLevel, socketNumToServer,
                                            transcodingTable, finishedDescribe);
}


void AsyncProxyServerMediaSession::continueAfterDESCRIBE(char const *sdpDescription) {
    describeCompletedFlag = 1;

    // Create a (client) "MediaSession" object from the stream's SDP description ("resultString"), then iterate through its
    // "MediaSubsession" objects, to set up corresponding "ServerMediaSubsession" objects that we'll use to serve the stream's tracks.
    do {
        fClientMediaSession = MediaSession::createNew(envir(), sdpDescription);
        if (fClientMediaSession == NULL) break;

        MediaSubsessionIterator iter(*fClientMediaSession);
        for (MediaSubsession *mss = iter.next(); mss != NULL; mss = iter.next()) {
            if (!allowProxyingForSubsession(*mss)) continue;

            ServerMediaSubsession *smss
                    = new AsyncProxyServerMediaSubsession(*mss, fInitialPortNum, fMultiplexRTCPWithRTP);
            addSubsession(smss);
            if (fVerbosityLevel > 0) {
                envir() << *this << " added new \"AsyncProxyServerMediaSubsession\" for "
                        << mss->protocolName() << "/" << mss->mediumName() << "/" << mss->codecName() << " track\n";
            }
        }
    } while (0);
    if (fProxyRTSPClient->fDoneDESCRIBE == False && fFinishedDescribe != NULL) {
        *fFinishedDescribe = True;
    }
}

////////// AsyncProxyServerMediaSubsession implementation //////////

UsageEnvironment &
operator<<(UsageEnvironment &env, const AsyncProxyServerMediaSubsession &psmss) { // used for debugging
    return env << "ProxyServerMediaSubsession[" << psmss.url() << "," << psmss.codecName() << "]";
}

AsyncProxyServerMediaSubsession::AsyncProxyServerMediaSubsession(MediaSubsession &mediaSubsession,
                                                                 portNumBits initialPortNum,
                                                                 Boolean multiplexRTCPWithRTP)
        : OnDemandServerMediaSubsession(mediaSubsession.parentSession().envir(), True/*reuseFirstSource*/,
                                        initialPortNum, multiplexRTCPWithRTP),
          fClientMediaSubsession(mediaSubsession), fCodecName(strDup(mediaSubsession.codecName())),
          fNext(NULL), fHaveSetupStream(False) {
}

AsyncProxyServerMediaSubsession::~AsyncProxyServerMediaSubsession() {

}

FramedSource *
AsyncProxyServerMediaSubsession::createNewStreamSource(unsigned int clientSessionId, unsigned int &estBitrate) {
    return nullptr;
}

void AsyncProxyServerMediaSubsession::closeStreamSource(FramedSource *inputSource) {
    OnDemandServerMediaSubsession::closeStreamSource(inputSource);
}

Groupsock *AsyncProxyServerMediaSubsession::createGroupsock(const sockaddr_storage &addr, Port port) {
    return OnDemandServerMediaSubsession::createGroupsock(addr, port);
}

RTCPInstance *
AsyncProxyServerMediaSubsession::createRTCP(Groupsock *RTCPgs, unsigned int totSessionBW, const unsigned char *cname,
                                            RTPSink *sink) {
    return OnDemandServerMediaSubsession::createRTCP(RTCPgs, totSessionBW, cname, sink);
}

void AsyncProxyServerMediaSubsession::subsessionByeHandler(void *clientData) {

}

void AsyncProxyServerMediaSubsession::subsessionByeHandler() {

}

int AsyncProxyServerMediaSubsession::verbosityLevel() const {
    return dynamic_cast<AsyncProxyServerMediaSession *>(fParentSession)->fVerbosityLevel;
}

RTPSink *
AsyncProxyServerMediaSubsession::createNewRTPSink(Groupsock *rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic,
                                                  FramedSource *inputSource) {
    if (verbosityLevel() > 0) {
        envir() << *this << "::createNewRTPSink()\n";
    }

    // Create (and return) the appropriate "RTPSink" object for our codec:
    // (Note: The configuration string might not be correct if a transcoder is used. FIX!) #####
    RTPSink *newSink;
    if (strcmp(fCodecName, "AC3") == 0 || strcmp(fCodecName, "EAC3") == 0) {
        newSink = AC3AudioRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic,
                                             fClientMediaSubsession.rtpTimestampFrequency());
#if 0 // This code does not work; do *not* enable it:
        } else if (strcmp(fCodecName, "AMR") == 0 || strcmp(fCodecName, "AMR-WB") == 0) {
    Boolean isWideband = strcmp(fCodecName, "AMR-WB") == 0;
    newSink = AMRAudioRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic,
                     isWideband, fClientMediaSubsession.numChannels());
#endif
    } else if (strcmp(fCodecName, "DV") == 0) {
        newSink = DVVideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
    } else if (strcmp(fCodecName, "GSM") == 0) {
        newSink = GSMAudioRTPSink::createNew(envir(), rtpGroupsock);
    } else if (strcmp(fCodecName, "H263-1998") == 0 || strcmp(fCodecName, "H263-2000") == 0) {
        newSink = H263plusVideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic,
                                                  fClientMediaSubsession.rtpTimestampFrequency());
    } else if (strcmp(fCodecName, "H264") == 0) {
        newSink = H264VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic,
                                              fClientMediaSubsession.fmtp_spropparametersets());
    } else if (strcmp(fCodecName, "H265") == 0) {
        newSink = H265VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic,
                                              fClientMediaSubsession.fmtp_spropvps(),
                                              fClientMediaSubsession.fmtp_spropsps(),
                                              fClientMediaSubsession.fmtp_sproppps());
    } else if (strcmp(fCodecName, "JPEG") == 0) {
        newSink = SimpleRTPSink::createNew(envir(), rtpGroupsock, 26, 90000, "video", "JPEG",
                                           1/*numChannels*/, False/*allowMultipleFramesPerPacket*/,
                                           False/*doNormalMBitRule*/);
    } else if (strcmp(fCodecName, "MP4A-LATM") == 0) {
        newSink = MPEG4LATMAudioRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic,
                                                   fClientMediaSubsession.rtpTimestampFrequency(),
                                                   fClientMediaSubsession.fmtp_config(),
                                                   fClientMediaSubsession.numChannels());
    } else if (strcmp(fCodecName, "MP4V-ES") == 0) {
        newSink = MPEG4ESVideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic,
                                                 fClientMediaSubsession.rtpTimestampFrequency(),
                                                 fClientMediaSubsession.attrVal_unsigned("profile-level-id"),
                                                 fClientMediaSubsession.fmtp_config());
    } else if (strcmp(fCodecName, "MPA") == 0) {
        newSink = MPEG1or2AudioRTPSink::createNew(envir(), rtpGroupsock);
    } else if (strcmp(fCodecName, "MPA-ROBUST") == 0) {
        newSink = MP3ADURTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
    } else if (strcmp(fCodecName, "MPEG4-GENERIC") == 0) {
        newSink = MPEG4GenericRTPSink::createNew(envir(), rtpGroupsock,
                                                 rtpPayloadTypeIfDynamic,
                                                 fClientMediaSubsession.rtpTimestampFrequency(),
                                                 fClientMediaSubsession.mediumName(),
                                                 fClientMediaSubsession.attrVal_str("mode"),
                                                 fClientMediaSubsession.fmtp_config(),
                                                 fClientMediaSubsession.numChannels());
    } else if (strcmp(fCodecName, "MPV") == 0) {
        newSink = MPEG1or2VideoRTPSink::createNew(envir(), rtpGroupsock);
    } else if (strcmp(fCodecName, "OPUS") == 0) {
        newSink = SimpleRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic,
                                           48000, "audio", "OPUS", 2, False/*only 1 Opus 'packet' in each RTP packet*/);
    } else if (strcmp(fCodecName, "T140") == 0) {
        newSink = T140TextRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
    } else if (strcmp(fCodecName, "THEORA") == 0) {
        newSink = TheoraVideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic,
                                                fClientMediaSubsession.fmtp_config());
    } else if (strcmp(fCodecName, "VORBIS") == 0) {
        newSink = VorbisAudioRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic,
                                                fClientMediaSubsession.rtpTimestampFrequency(),
                                                fClientMediaSubsession.numChannels(),
                                                fClientMediaSubsession.fmtp_config());
    } else if (strcmp(fCodecName, "VP8") == 0) {
        newSink = VP8VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
    } else if (strcmp(fCodecName, "VP9") == 0) {
        newSink = VP9VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
    } else if (strcmp(fCodecName, "AMR") == 0 || strcmp(fCodecName, "AMR-WB") == 0) {
        // Proxying of these codecs is currently *not* supported, because the data received by the "RTPSource" object is not in a
        // form that can be fed directly into a corresponding "RTPSink" object.
        if (verbosityLevel() > 0) {
            envir() << "\treturns NULL (because we currently don't support the proxying of \""
                    << fClientMediaSubsession.mediumName() << "/" << fCodecName << "\" streams)\n";
        }
        return NULL;
    } else if (strcmp(fCodecName, "QCELP") == 0 ||
               strcmp(fCodecName, "H261") == 0 ||
               strcmp(fCodecName, "H263-1998") == 0 || strcmp(fCodecName, "H263-2000") == 0 ||
               strcmp(fCodecName, "X-QT") == 0 || strcmp(fCodecName, "X-QUICKTIME") == 0) {
        // This codec requires a specialized RTP payload format; however, we don't yet have an appropriate "RTPSink" subclass for it:
        if (verbosityLevel() > 0) {
            envir() << "\treturns NULL (because we don't have a \"RTPSink\" subclass for this RTP payload format)\n";
        }
        return NULL;
    } else {
        // This codec is assumed to have a simple RTP payload format that can be implemented just with a "SimpleRTPSink":
        Boolean allowMultipleFramesPerPacket = True; // by default
        Boolean doNormalMBitRule = True; // by default
        // Some codecs change the above default parameters:
        if (strcmp(fCodecName, "MP2T") == 0) {
            doNormalMBitRule = False; // no RTP 'M' bit
        }
        newSink = SimpleRTPSink::createNew(envir(), rtpGroupsock,
                                           rtpPayloadTypeIfDynamic, fClientMediaSubsession.rtpTimestampFrequency(),
                                           fClientMediaSubsession.mediumName(), fCodecName,
                                           fClientMediaSubsession.numChannels(), allowMultipleFramesPerPacket,
                                           doNormalMBitRule);
    }

    // Because our relayed frames' presentation times are inaccurate until the input frames have been RTCP-synchronized,
    // we temporarily disable RTCP "SR" reports for this "RTPSink" object:
    newSink->enableRTCPReports() = False;

    // Also tell our "PresentationTimeSubsessionNormalizer" object about the "RTPSink", so it can enable RTCP "SR" reports later:
    PresentationTimeSubsessionNormalizer *ssNormalizer;
    if (strcmp(fCodecName, "H264") == 0 ||
        strcmp(fCodecName, "H265") == 0 ||
        strcmp(fCodecName, "MP4V-ES") == 0 ||
        strcmp(fCodecName, "MPV") == 0 ||
        strcmp(fCodecName, "DV") == 0) {
        // There was a separate 'framer' object in front of the "PresentationTimeSubsessionNormalizer", so go back one object to get it:
        ssNormalizer = (PresentationTimeSubsessionNormalizer *) (((FramedFilter *) inputSource)->inputSource());
    } else {
        ssNormalizer = (PresentationTimeSubsessionNormalizer *) inputSource;
    }
    ssNormalizer->setRTPSink(newSink);

    return newSink;
}

char const *AsyncProxyServerMediaSubsession::url() const {
    { return dynamic_cast<AsyncProxyServerMediaSession *>(fParentSession)->url(); }
}



////////// "AsyncProxyRTSPClient" implementation /////////

AsyncProxyRTSPClient *
defaultCreateNewAsyncProxyRTSPClientFunc(AsyncProxyServerMediaSession &ourServerMediaSession,
                                         char const *rtspURL,
                                         char const *username, char const *password,
                                         portNumBits tunnelOverHTTPPortNum, int verbosityLevel,
                                         int socketNumToServer) {
    return new AsyncProxyRTSPClient(ourServerMediaSession, rtspURL, username, password,
                                    tunnelOverHTTPPortNum, verbosityLevel, socketNumToServer);
}

AsyncProxyRTSPClient::~AsyncProxyRTSPClient() {
    delete fOurAuthenticator;
}

void AsyncProxyRTSPClient::sendDESCRIBE() {
    sendDescribeCommand(::continueAfterDESCRIBE, auth());
}

AsyncProxyRTSPClient::AsyncProxyRTSPClient(ProxyServerMediaSession &ourServerMediaSession, const char *rtspURL,
                                           const char *username, const char *password,
                                           portNumBits tunnelOverHTTPPortNum, int verbosityLevel, int socketNumToServer)
        : ProxyRTSPClient(ourServerMediaSession, rtspURL, username, password, tunnelOverHTTPPortNum, verbosityLevel,
                          socketNumToServer) {
    if (username != NULL && password != NULL) {
        fOurAuthenticator = new Authenticator(username, password);
    } else {
        fOurAuthenticator = NULL;
    }
}

void AsyncProxyRTSPClient::continueAfterDESCRIBE(const char *sdpDescription) {
    ProxyRTSPClient::continueAfterDESCRIBE(sdpDescription);
}

UsageEnvironment &operator<<(UsageEnvironment &env, const AsyncProxyRTSPClient &proxyRTSPClient) { // used for debugging
    return env << "AsyncProxyRTSPClient[" << proxyRTSPClient.url() << "]";
}