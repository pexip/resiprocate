#ifndef _SIMPLE_DUM_H_
#define _SIMPLE_DUM_H_

#if defined(_MSC_VER)
#  define SIMPLEDUM_EXPORT __declspec(dllexport)
#else
#  define SIMPLEDUM_EXPORT extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _SimpleDum SimpleDum;

SIMPLEDUM_EXPORT
SimpleDum * simple_dum_new ();

SIMPLEDUM_EXPORT
void simple_dum_free (SimpleDum * simpledum);

/***
 * s_conference: VMR name (expected format: "sip:<vmr>@nigthly.pexip.com")
 * s_identity: YOU (expected format: "sip:<myuser>@nigthly.pexip.com")
 * s_proxy: the proxy to use: (expected format: "sip:nightly.pexip.com")
 * local_sdp: our locally crafted SDP
 * remote_sdp: The returned SDP
 */
SIMPLEDUM_EXPORT
int simple_dum_connect_sip(SimpleDum * simpledum, const char * s_conference, const char * s_identity, const char * s_proxy, const char * local_sdp, char ** remote_sdp);

SIMPLEDUM_EXPORT
void simple_dum_disconnect_sip(SimpleDum * simpledum);


#ifdef __cplusplus
}
#endif

#endif /* _SIMPLE_DUM_H_ */