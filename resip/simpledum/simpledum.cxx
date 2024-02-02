#include "simpledum.h"

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
      std::string mRemoteSDP;

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
         mRemoteSDP = ss.str ();
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

      }

      virtual void onMessageSuccess(InviteSessionHandle is, const SipMessage& msg)
      {
         cout << name << ": InviteSession-onMessageSuccess - " << msg.brief() << endl;
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


struct _SimpleDum {
   SipStack stackUac;
   DialogUsageManager* dumUac;   

   TestUac mUAC;
   std::string remote_sdp;
   std::thread sipThread;
   bool quit;

   _SimpleDum ()
   : stackUac()
   , dumUac(nullptr)
   , mUAC()
   , remote_sdp()
   , sipThread()
   , quit(false)
   {}
};


static void
init_simpledum()
{
   static bool _did_init = false;
   if (!_did_init) {
      _did_init = true;
      Log::initialize(Log::Cout, resip::Log::Info, nullptr);
   }
}

SimpleDum * simple_dum_new ()
{
   return new _SimpleDum();
}

void simple_dum_free (SimpleDum * simpledum)
{
   delete simpledum;
}

void sipStateThread(SimpleDum * simpledum)
{
   while (!simpledum->quit){
        simpledum->stackUac.process(50);
        while(simpledum->dumUac->process());
   }   
}

/***
 * s_conference: VMR name (expected format: "sip:<vmr>@nigthly.pexip.com")
 * s_identity: YOU (expected format: "sip:<myuser>@nigthly.pexip.com")
 * s_proxy: the proxy to use: (expected format: "sip:nightly.pexip.com")
 * local_sdp: our locally crafted SDP
 * remote_sdp: The returned SDP
 */
int simple_dum_connect_sip(SimpleDum * simpledum, const char * s_conference, const char * s_identity, const char * s_proxy, const char * local_sdp, char ** remote_sdp)
{
   assert(s_conference);
   assert(s_identity);
   assert(s_proxy);
   assert (remote_sdp);

   init_simpledum ();


   NameAddr conference = NameAddr(s_conference);
   NameAddr me = NameAddr(s_identity);
   Uri proxy = Uri(Data(s_proxy));

   Data* txt = new Data(local_sdp);
   HeaderFieldValue* hfv = new HeaderFieldValue(txt->data(), (unsigned int)txt->size());
   Mime type("application", "sdp");
   SdpContents * mSdp = new SdpContents(*hfv, type);

   //set up UAC
   //SipStack stackUac;
   simpledum->dumUac = new DialogUsageManager(simpledum->stackUac);
   simpledum->stackUac.addTransport(TCP, 12005);
   SharedPtr<MasterProfile> uacMasterProfile(new MasterProfile);
   simpledum->dumUac->setMasterProfile(uacMasterProfile);
   simpledum->dumUac->setInviteSessionHandler(&simpledum->mUAC);
   simpledum->dumUac->setClientRegistrationHandler(&simpledum->mUAC);
   simpledum->dumUac->addOutOfDialogHandler(OPTIONS, &simpledum->mUAC);

   auto_ptr<AppDialogSetFactory> uac_dsf(new testAppDialogSetFactory);
   simpledum->dumUac->setAppDialogSetFactory(uac_dsf);

   simpledum->dumUac->getMasterProfile()->setOutboundProxy(proxy);
   simpledum->dumUac->getMasterProfile()->addSupportedOptionTag(Token(Symbols::Outbound));

   simpledum->dumUac->getMasterProfile()->setDefaultFrom(me);

   simpledum->dumUac->send(simpledum->dumUac->makeInviteSession(conference, mSdp, new testAppDialogSet(*simpledum->dumUac, "UAC(INVITE)")));

   simpledum->sipThread = std::thread(sipStateThread, simpledum);

   while (simpledum->mUAC.mRemoteSDP.empty()){
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  *remote_sdp = strdup (simpledum->mUAC.mRemoteSDP.c_str());

   delete mSdp;
   delete txt;
   delete hfv;

   return 0;
}

void simple_dum_disconnect_sip(SimpleDum * simpledum){
   simpledum->dumUac->shutdown(NULL);
   simpledum->quit = true;
   simpledum->sipThread.join();
}