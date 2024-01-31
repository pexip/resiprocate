#include "resip/stack/SdpContents.hxx"
#include "resip/stack/PlainContents.hxx"
#include "resip/stack/SipMessage.hxx"
#include "resip/stack/ShutdownMessage.hxx"
#include "resip/stack/SipStack.hxx"
#include "resip/dum/ClientAuthManager.hxx"
#include "resip/dum/ClientInviteSession.hxx"
#include "resip/dum/ClientRegistration.hxx"
#include "resip/dum/DialogUsageManager.hxx"
#include "resip/dum/DumShutdownHandler.hxx"
#include "resip/dum/InviteSessionHandler.hxx"
#include "resip/dum/MasterProfile.hxx"
#include "resip/dum/RegistrationHandler.hxx"
#include "resip/dum/ServerInviteSession.hxx"
#include "resip/dum/ServerOutOfDialogReq.hxx"
#include "resip/dum/OutOfDialogHandler.hxx"
#include "resip/dum/AppDialog.hxx"
#include "resip/dum/AppDialogSet.hxx"
#include "resip/dum/AppDialogSetFactory.hxx"
#include "rutil/Log.hxx"
#include "rutil/Logger.hxx"
#include "rutil/Random.hxx"
#include "rutil/WinLeakCheck.hxx"

#include <sstream>
#include <time.h>
#include <thread>

using namespace resip;
using namespace std;

struct sipState {
   SipStack stackUac;
   DialogUsageManager* dumUac;   
   const char * remote_sdp;
   std::thread sipThread;
   bool quit;

   sipState ()
   : stackUac()
   {}

};
static struct sipState g_state;

class testAppDialog : public AppDialog
{
public:
   testAppDialog(HandleManager& ham, Data &SampleAppData) : AppDialog(ham), mSampleAppData(SampleAppData)
   {  
      cout << mSampleAppData << ": testAppDialog: created." << endl;  
   }
   virtual ~testAppDialog() 
   { 
      cout << mSampleAppData << ": testAppDialog: destroyed." << endl; 
   }
   Data mSampleAppData;
};

class testAppDialogSet : public AppDialogSet
{
public:
   testAppDialogSet(DialogUsageManager& dum, Data SampleAppData) : AppDialogSet(dum), mSampleAppData(SampleAppData)
   {  
      cout << mSampleAppData << ": testAppDialogSet: created." << endl;  
   }
   virtual ~testAppDialogSet() 
   {  
      cout << mSampleAppData << ": testAppDialogSet: destroyed." << endl;  
   }
   virtual AppDialog* createAppDialog(const SipMessage& msg) 
   {  
      return new testAppDialog(mDum, mSampleAppData);  
   }
   virtual SharedPtr<UserProfile> selectUASUserProfile(const SipMessage& msg) 
   { 
      cout << mSampleAppData << ": testAppDialogSet: UAS UserProfile requested for msg: " << msg.brief() << endl;  
      return mDum.getMasterUserProfile(); 
   }
   Data mSampleAppData;
};

class testAppDialogSetFactory : public AppDialogSetFactory
{
public:
   virtual AppDialogSet* createAppDialogSet(DialogUsageManager& dum, const SipMessage& msg) 
   {  return new testAppDialogSet(dum, Data("UAS") + Data("(") + getMethodName(msg.header(h_RequestLine).getMethod()) + Data(")"));  }
   // For a UAS the testAppDialogSet will be created by DUM using this function.  If you want to set 
   // Application Data, then one approach is to wait for onNewSession(ServerInviteSessionHandle ...) 
   // to be called, then use the ServerInviteSessionHandle to get at the AppDialogSet or AppDialog,
   // then cast to your derived class and set the desired application data.
};


// Generic InviteSessionHandler
class TestInviteSessionHandler : public InviteSessionHandler, public ClientRegistrationHandler, public OutOfDialogHandler
{
   public:
      Data name;
      bool registered;
      ClientRegistrationHandle registerHandle;
      
      TestInviteSessionHandler(const Data& n) : name(n), registered(false) 
      {
      }

      virtual ~TestInviteSessionHandler()
      {
      }
      
      virtual void onSuccess(ClientRegistrationHandle h, const SipMessage& response)
      {         
         registerHandle = h;   
         assert(registerHandle.isValid());         
         cout << name << ": ClientRegistration-onSuccess - " << response.brief() << endl;
         registered = true;
      }

      virtual void onFailure(ClientRegistrationHandle, const SipMessage& msg)
      {
         cout << name << ": ClientRegistration-onFailure - " << msg.brief() << endl;
         throw;  // Ungracefully end
      }

      virtual void onRemoved(ClientRegistrationHandle, const SipMessage& response)
      {
          cout << name << ": ClientRegistration-onRemoved" << endl;
      }

      virtual int onRequestRetry(ClientRegistrationHandle, int retrySeconds, const SipMessage& response)
      {
          cout << name << ": ClientRegistration-onRequestRetry (" << retrySeconds << ") - " << response.brief() << endl;
          return -1;
      }

      virtual void onNewSession(ClientInviteSessionHandle, InviteSession::OfferAnswerType oat, const SipMessage& msg)
      {
         cout << name << ": ClientInviteSession-onNewSession - " << msg.brief() << endl;
      }
      
      virtual void onNewSession(ServerInviteSessionHandle, InviteSession::OfferAnswerType oat, const SipMessage& msg)
      {
         cout << name << ": ServerInviteSession-onNewSession - " << msg.brief() << endl;
      }

      virtual void onFailure(ClientInviteSessionHandle, const SipMessage& msg)
      {
         cout << name << ": ClientInviteSession-onFailure - " << msg.brief() << endl;
      }
      
      virtual void onProvisional(ClientInviteSessionHandle, const SipMessage& msg)
      {
         cout << name << ": ClientInviteSession-onProvisional - " << msg.brief() << endl;
      }

      virtual void onConnected(ClientInviteSessionHandle, const SipMessage& msg)
      {
         cout << name << ": ClientInviteSession-onConnected - " << msg.brief() << endl;
      }

      virtual void onStaleCallTimeout(ClientInviteSessionHandle handle)
      {
         cout << name << ": ClientInviteSession-onStaleCallTimeout" << endl;
      }

      virtual void onConnected(InviteSessionHandle, const SipMessage& msg)
      {
         cout << name << ": InviteSession-onConnected - " << msg.brief() << endl;
      }

      virtual void onRedirected(ClientInviteSessionHandle, const SipMessage& msg)
      {
         cout << name << ": ClientInviteSession-onRedirected - " << msg.brief() << endl;
      }

      virtual void onTerminated(InviteSessionHandle, InviteSessionHandler::TerminatedReason reason, const SipMessage* msg)
      {
         cout << name << ": InviteSession-onTerminated - " << msg->brief() << endl;
         assert(0); // This is overrideen in UAS and UAC specific handlers
      }

      virtual void onAnswer(InviteSessionHandle, const SipMessage& msg, const SdpContents& sdp)
      {
         cout << name << ": InviteSession-onAnswer(SDP)" << endl;
         //sdp->encode(cout);
      }

      virtual void onOffer(InviteSessionHandle is, const SipMessage& msg, const SdpContents& sdp)      
      {
         cout << name << ": InviteSession-onOffer(SDP)" << endl;
         //sdp->encode(cout);
      }

      virtual void onEarlyMedia(ClientInviteSessionHandle, const SipMessage& msg, const SdpContents& sdp)
      {
         cout << name << ": InviteSession-onEarlyMedia(SDP)" << endl;
         //sdp->encode(cout);
      }

      virtual void onOfferRequired(InviteSessionHandle, const SipMessage& msg)
      {
         cout << name << ": InviteSession-onOfferRequired - " << msg.brief() << endl;
      }

      virtual void onOfferRejected(InviteSessionHandle, const SipMessage* msg)
      {
         cout << name << ": InviteSession-onOfferRejected" << endl;
      }

      virtual void onRefer(InviteSessionHandle, ServerSubscriptionHandle, const SipMessage& msg)
      {
         cout << name << ": InviteSession-onRefer - " << msg.brief() << endl;
      }

      virtual void onReferAccepted(InviteSessionHandle, ClientSubscriptionHandle, const SipMessage& msg)
      {
         cout << name << ": InviteSession-onReferAccepted - " << msg.brief() << endl;
      }

      virtual void onReferRejected(InviteSessionHandle, const SipMessage& msg)
      {
         cout << name << ": InviteSession-onReferRejected - " << msg.brief() << endl;
      }

      virtual void onReferNoSub(InviteSessionHandle, const SipMessage& msg)
      {
         cout << name << ": InviteSession-onReferNoSub - " << msg.brief() << endl;
      }

      virtual void onInfo(InviteSessionHandle, const SipMessage& msg)
      {
         cout << name << ": InviteSession-onInfo - " << msg.brief() << endl;
      }

      virtual void onInfoSuccess(InviteSessionHandle, const SipMessage& msg)
      {
         cout << name << ": InviteSession-onInfoSuccess - " << msg.brief() << endl;
      }

      virtual void onInfoFailure(InviteSessionHandle, const SipMessage& msg)
      {
         cout << name << ": InviteSession-onInfoFailure - " << msg.brief() << endl;
      }

      virtual void onMessage(InviteSessionHandle, const SipMessage& msg)
      {
         cout << name << ": InviteSession-onMessage - " << msg.brief() << endl;
      }

      virtual void onMessageSuccess(InviteSessionHandle, const SipMessage& msg)
      {
         cout << name << ": InviteSession-onMessageSuccess - " << msg.brief() << endl;
      }

      virtual void onMessageFailure(InviteSessionHandle, const SipMessage& msg)
      {
         cout << name << ": InviteSession-onMessageFailure - " << msg.brief() << endl;
      }

      virtual void onForkDestroyed(ClientInviteSessionHandle)
     {
         cout << name << ": ClientInviteSession-onForkDestroyed" << endl;
     }

      // Out-of-Dialog Callbacks
      virtual void onSuccess(ClientOutOfDialogReqHandle, const SipMessage& successResponse)
      {
          cout << name << ": ClientOutOfDialogReq-onSuccess - " << successResponse.brief() << endl;
      }
      virtual void onFailure(ClientOutOfDialogReqHandle, const SipMessage& errorResponse)
      {
          cout << name << ": ClientOutOfDialogReq-onFailure - " << errorResponse.brief() << endl;
      }
      virtual void onReceivedRequest(ServerOutOfDialogReqHandle ood, const SipMessage& request)
      {
          cout << name << ": ServerOutOfDialogReq-onReceivedRequest - " << request.brief() << endl;
          // Add SDP to response here if required
          cout << name << ": Sending 200 response to OPTIONS." << endl;
          ood->send(ood->answerOptions());
      }
};

class TestUac : public TestInviteSessionHandler
{
   public:
      bool done;
      int mNumExpectedMessages;

      TestUac() 
         : TestInviteSessionHandler("UAC"), 
           done(false),
           mNumExpectedMessages(2)
      {
      }

      virtual ~TestUac()
      {
//         assert(mNumExpectedMessages == 0);
      }

      virtual void onOffer(InviteSessionHandle is, const SipMessage& msg, const SdpContents& sdp)      
      {
         cout << name << ": InviteSession-onOffer(SDP)" << endl;

         std::stringstream ss;
         ss << sdp << endl;
         g_state.remote_sdp = ss.str ().c_str ();
         is->provideAnswer(sdp);
      }

      using TestInviteSessionHandler::onConnected;
      virtual void onConnected(ClientInviteSessionHandle is, const SipMessage& msg)
      {
         cout << name << ": ClientInviteSession-onConnected - " << msg.brief() << endl;
         cout << "Connected now - requestingOffer from UAS" << endl;
         is->requestOffer();

         // At this point no NIT should have been sent
         assert(!is->getLastSentNITRequest());

         // Send a first MESSAGE from UAC with some contents (we use a fake PlainContents contents here for
         // simplicity)
         PlainContents contents("Hi there!!!");
         is->message(contents);

         // Immediately send another one, which will end up queued on the
         // InviteSession's NIT queue
         PlainContents contentsOther("Hi again!!!");
         is->message(contentsOther);
      }

      virtual void onMessageSuccess(InviteSessionHandle is, const SipMessage& msg)
      {
         cout << name << ": InviteSession-onMessageSuccess - " << msg.brief() << endl;

         assert(is->getLastSentNITRequest());
         PlainContents* pContents = dynamic_cast<PlainContents*>(is->getLastSentNITRequest()->getContents());
         assert(pContents != NULL);

         if(mNumExpectedMessages == 2)
         {
            assert(pContents->text() == Data("Hi there!!!"));
            mNumExpectedMessages--;
         }
         else if(mNumExpectedMessages == 1)
         {
            assert(pContents->text() == Data("Hi again!!!"));
            mNumExpectedMessages--;
         }
      }

      virtual void onInfo(InviteSessionHandle is, const SipMessage& msg)
      {
         cout << name << ": InviteSession-onInfo - " << msg.brief() << endl;
         is->acceptNIT();
      }

      virtual void onTerminated(InviteSessionHandle, InviteSessionHandler::TerminatedReason reason, const SipMessage* msg)
      {
         if(msg)
         {
            cout << name << ": InviteSession-onTerminated - " << msg->brief() << endl;
         }
         else
         {
            cout << name << ": InviteSession-onTerminated" << endl;
         }
         done = true;
      }
};

void sipStateThread(std::string meh)
{
   while (!g_state.quit){
        g_state.stackUac.process(50);
        while(g_state.dumUac->process());
   }   
}


/***
 * s_conference: VMR name (expected format: "sip:<vmr>@nigthly.pexip.com")
 * s_identity: YOU (expected format: "sip:<myuser>@nigthly.pexip.com")
 * s_proxy: the proxy to use: (expected format: "sip:nightly.pexip.com")
 * local_sdp: our locally crafted SDP
 * remote_sdp: The returned SDP
 */

int connect_sip(const char * s_conference, const char * s_identity, const char * s_proxy, const char * local_sdp, const char ** remote_sdp)
{
   assert(s_conference);
   assert(s_identity);
   assert(s_proxy);

   NameAddr conference = NameAddr(s_conference);
   NameAddr me = NameAddr(s_identity);
   Uri proxy = Uri(Data(s_proxy));

   Data* txt = new Data(local_sdp);
   HeaderFieldValue* hfv = new HeaderFieldValue(txt->data(), (unsigned int)txt->size());
   Mime type("application", "sdp");
   SdpContents * mSdp = new SdpContents(*hfv, type);

   //set up UAC
   //SipStack stackUac;
   g_state.dumUac = new DialogUsageManager(g_state.stackUac);
   g_state.stackUac.addTransport(TCP, 12005);
   SharedPtr<MasterProfile> uacMasterProfile(new MasterProfile);
   g_state.dumUac->setMasterProfile(uacMasterProfile);

   TestUac uac;
   g_state.dumUac->setInviteSessionHandler(&uac);
   g_state.dumUac->setClientRegistrationHandler(&uac);
   g_state.dumUac->addOutOfDialogHandler(OPTIONS, &uac);

   auto_ptr<AppDialogSetFactory> uac_dsf(new testAppDialogSetFactory);
   g_state.dumUac->setAppDialogSetFactory(uac_dsf);

   g_state.dumUac->getMasterProfile()->setOutboundProxy(proxy);
   g_state.dumUac->getMasterProfile()->addSupportedOptionTag(Token(Symbols::Outbound));

   g_state.dumUac->getMasterProfile()->setDefaultFrom(me);


   g_state.dumUac->send(g_state.dumUac->makeInviteSession(conference, mSdp, new testAppDialogSet(*g_state.dumUac, "UAC(INVITE)")));

   g_state.sipThread = std::thread(sipStateThread, "meh");

   while (g_state.remote_sdp == NULL){
      sleep(1);
  }
  *remote_sdp = g_state.remote_sdp;

   delete mSdp;
   delete txt;
   delete hfv;

   return 0;
}

void disconnect_sip(){
   g_state.dumUac->shutdown(NULL);
   g_state.quit = true;
   g_state.sipThread.join();
}

static const char * local_sdp = "v=0\r\n"
   "o=_pmx_test_framework_ 0000000 1 IN IP4 5.6.7.8\r\n"
   "s=-\r\n"
   "t=0 0\r\n"
   "a=extmap-allow-mixed\r\n"
   "a=msid-semantic: WMS _pmx_test_framework_\r\n"
   "a=group:BUNDLE 0 1 2\r\n"
   "m=audio 6666 UDP/TLS/RTP/SAVPF 96 126\r\n"
   "c=IN IP4 1.2.3.4\r\n"
   "a=sendrecv\r\n"
   "a=mid:0\r\n"
   "a=msid:_pmx_test_framework_ 0\r\n"
   "a=candidate:1 1 udp 0 1.2.3.4 6666 typ host\r\n"
   "a=ice-ufrag:testuser\r\n"
   "a=ice-pwd:testpass\r\n"
   "a=ice-options:trickle\r\n"
   "a=connection:new\r\n"
   "a=rtpmap:96 OPUS/48000/2\r\n"
   "a=rtpmap:126 TELEPHONE-EVENT/48000\r\n"
   "a=fmtp:96 maxplaybackrate=48000;stereo=1;useinbandfec=1\r\n"
   "a=fingerprint:sha-256 DE:AD:BE:EF:DE:AD:BE:EF:DE:AD:BE:EF:DE:AD:BE:EF:DE:AD:BE:EF:DE:AD:BE:EF:DE:AD:BE:EF:DE:AD:BE:EF\r\n"
   "a=content:main\r\n"
   "a=extmap:3 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01\r\n"
   "a=rtcp:6666 IN IP4 1.2.3.4\r\n"
   "a=rtcp-fb:* transport-cc\r\n"
   "a=rtcp-mux\r\n"
   "a=ssrc:1000 label:audio\r\n"
   "a=setup:active\r\n"
   "m=video 9 UDP/TLS/RTP/SAVPF 97 120 123\r\n"
   "c=IN IP4 0.0.0.0\r\n"
   "a=bundle-only\r\n"
   "a=sendrecv\r\n"
   "a=mid:1\r\n"
   "a=msid:_pmx_test_framework_ 1\r\n"
   "a=candidate:1 1 udp 0 1.2.3.4 6666 typ host\r\n"
   "a=ice-ufrag:testuser\r\n"
   "a=ice-pwd:testpass\r\n"
   "a=ice-options:trickle\r\n"
   "a=connection:new\r\n"
   "a=rtpmap:97 H264/90000\r\n"
   "a=rtpmap:120 RTX/90000\r\n"
   "a=rtpmap:123 PEXULPFEC/90000\r\n"
   "a=fmtp:97 profile-level-id=428014;max-br=1660;max-mbps=108000;max-fs=3600;max-smbps=108000;max-fps=3000\r\n"
   "a=fmtp:120 apt=97\r\n"
   "a=fingerprint:sha-256 DE:AD:BE:EF:DE:AD:BE:EF:DE:AD:BE:EF:DE:AD:BE:EF:DE:AD:BE:EF:DE:AD:BE:EF:DE:AD:BE:EF:DE:AD:BE:EF\r\n"
   "a=content:main\r\n"
   "a=extmap:3 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01\r\n"
   "a=rtcp:9 IN IP4 0.0.0.0\r\n"
   "a=rtcp-fb:* nack\r\n"
   "a=rtcp-fb:* nack pli\r\n"
   "a=rtcp-fb:* transport-cc\r\n"
   "a=rtcp-fb:* goog-remb\r\n"
   "a=rtcp-mux\r\n"
   "a=ssrc:2000 label:video\r\n"
   "a=ssrc:2001 label:video\r\n"
   "a=ssrc:2002 label:video\r\n"
   "a=ssrc-group:FID 2000 2001\r\n"
   "a=ssrc-group:FEC-FR 2000 2002\r\n"
   "a=setup:active\r\n"
   "m=video 9 UDP/TLS/RTP/SAVPF 97 120 123\r\n"
   "c=IN IP4 0.0.0.0\r\n"
   "a=bundle-only\r\n"
   "a=sendrecv\r\n"
   "a=mid:2\r\n"
   "a=msid:_pmx_test_framework_ 2\r\n"
   "a=candidate:1 1 udp 0 1.2.3.4 6666 typ host\r\n"
   "a=ice-ufrag:testuser\r\n"
   "a=ice-pwd:testpass\r\n"
   "a=ice-options:trickle\r\n"
   "a=connection:new\r\n"
   "a=rtpmap:97 H264/90000\r\n"
   "a=rtpmap:120 RTX/90000\r\n"
   "a=rtpmap:123 PEXULPFEC/90000\r\n"
   "a=fmtp:97 profile-level-id=428014;max-br=1660;max-mbps=108000;max-fs=3600;max-smbps=108000;max-fps=3000\r\n"
   "a=fmtp:120 apt=97\r\n"
   "a=fingerprint:sha-256 DE:AD:BE:EF:DE:AD:BE:EF:DE:AD:BE:EF:DE:AD:BE:EF:DE:AD:BE:EF:DE:AD:BE:EF:DE:AD:BE:EF:DE:AD:BE:EF\r\n"
   "a=content:slides\r\n"
   "a=extmap:3 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01\r\n"
   "a=rtcp:9 IN IP4 0.0.0.0\r\n"
   "a=rtcp-fb:* nack\r\n"
   "a=rtcp-fb:* nack pli\r\n"
   "a=rtcp-fb:* transport-cc\r\n"
   "a=rtcp-fb:* goog-remb\r\n"
   "a=rtcp-mux\r\n"
   "a=ssrc:3000 label:video\r\n"
   "a=ssrc:3001 label:video\r\n"
   "a=ssrc:3002 label:video\r\n"
   "a=ssrc-group:FID 3000 3001\r\n"
   "a=ssrc-group:FEC-FR 3000 3002\r\n"
   "a=setup:active\r\n";

int 
main (int argc, char** argv)
{
   Log::initialize(Log::Cout, resip::Log::Info, argv[0]);

   const char * remote_sdp = NULL;
   connect_sip("sip:meet.knut.saastad@nightly.pexip.com", "sip:SomeOne@nightly.pexip.com", "sip:nightly.pexip.com", local_sdp, &remote_sdp);

   cout << "MAIN: SDP :" << remote_sdp;

   sleep(5);

   cout << "stopping" << endl;

   disconnect_sip();
}