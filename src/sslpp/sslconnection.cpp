/*
 * Copyright 2002 Lite Speed Technologies Inc, All Rights Reserved.
 * LITE SPEED PROPRIETARY/CONFIDENTIAL.
 */


#include "sslconnection.h"

#include <log4cxx/logger.h>
#include <lsr/ls_pool.h>
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>

#include <sslpp/sslerror.h>
#include <sslpp/sslsesscache.h>
#include <sslpp/sslcert.h>
#include <util/stringtool.h>

#include <errno.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>

#define DEBUGGING

#ifdef DEBUGGING

#if TEST_PGM
#define DEBUG_MESSAGE(...)     printf(__VA_ARGS__);
#define ERROR_MESSAGE(...)     fprintf(stderr, __VA_ARGS__);
#else
#define DEBUG_MESSAGE(...)     LS_DBG_L(__VA_ARGS__)
#define ERROR_MESSAGE(...)     LS_ERROR(__VA_ARGS__)
#endif

#else

#define DEBUG_MESSAGE(...)
#define ERROR_MESSAGE(...)

#endif

int32_t SslConnection::s_iConnIdx = -1;

SslConnection::SslConnection()
    : m_ssl(NULL)
    , m_pSessCache(NULL)
    , m_flag(0)
    , m_iStatus(DISCONNECTED)
    , m_iWant(0)
{
    ls_fdbuf_bio_init(&m_bio);
}


SslConnection::~SslConnection()
{
    if (m_ssl)
        release();
}


void SslConnection::setSSL(SSL *ssl)
{
    assert(!m_ssl);
    m_ssl = ssl;
    m_flag = 0;
    SSL_set_ex_data(ssl, s_iConnIdx, (void *)this);
}

void SslConnection::release()
{
    assert(m_ssl);
    if (m_iStatus != DISCONNECTED)
        shutdown(0);

    SSL_free(m_ssl);
    m_ssl = NULL;
    m_iWant = 0;
}


int SslConnection::setfd(int fd)
{
    // Note that fd is not really used.
    m_iWant = 0;
    DEBUG_MESSAGE("[SSL: %p] setfd: %d\n", this, fd);
    if (m_ssl)
    {
        BIO * bio = ls_fdbio_create(fd, &m_bio);
        SSL_set_bio(m_ssl, bio, bio);
    }
    return 0;
}


int SslConnection::read(char *pBuf, int len)
{
    assert(m_ssl);
    m_iWant = 0;
    char *p = pBuf;
    char *pEnd = pBuf + len;
    int ret;
    m_bio.m_rbioWaitEvent = 0;
    while(p < pEnd && (m_bio.m_rbioIndex < m_bio.m_rbioBuffered
                       || !m_bio.m_rbioWaitEvent))
    {
        DEBUG_MESSAGE("[SSL: %p] SSL_read\n", this);
        ret = SSL_read(m_ssl, p, pEnd - p);
        DEBUG_MESSAGE("[SSL: %p] SSL_read returned %d\n", this,
                      ret);
        if (ret > 0)
        {
            p += ret;
        }
        else
        {
            m_iWant = LAST_READ;
            if (p == pBuf)
                return checkError(ret);
            break;
        }
    }
    return p - pBuf;
}


int SslConnection::write(const char *pBuf, int len)
{
    assert(m_ssl);
    m_iWant = 0;
    if (len <= 0)
        return 0;
    int ret = SSL_write(m_ssl, pBuf, len);

    LS_DBG_M("SSL_write( %p, %p, %d) return %d, pending %d\n",
             m_ssl, pBuf, len, ret, wpending());

    if (ret > 0)
    {
        m_iWant &= ~LAST_WRITE;
        return ret;
    }
    else
    {
        LS_DBG_M("SslError:%s\n", SslError(SSL_get_error(m_ssl, ret)).what());
        m_iWant = LAST_WRITE;
        return checkError(ret);
    }
}


int SslConnection::writev(const struct iovec *vect, int count,
                          int *finished)
{
    int ret = 0;

    const struct iovec *pEnd = vect + count;
    const char *pBuf;
    int bufSize;
    int written;

    char *pBufEnd;
    char *pCurEnd;
    char achBuf[4096];
    
    pBufEnd = achBuf + 4096;
    pCurEnd = achBuf;
    for (; vect < pEnd ;)
    {
        pBuf = (const char *) vect->iov_base;
        bufSize = vect->iov_len;
        if (bufSize < 1024)
        {
            if (pBufEnd - pCurEnd > bufSize)
            {
                memmove(pCurEnd, pBuf, bufSize);
                pCurEnd += bufSize;
                ++vect;
                if (vect < pEnd)
                    continue;
            }
            pBuf = achBuf;
            bufSize = pCurEnd - pBuf;
            pCurEnd = achBuf;
        }
        else if (pCurEnd != achBuf)
        {
            pBuf = achBuf;
            bufSize = pCurEnd - pBuf;
            pCurEnd = achBuf;
        }
        else
            ++vect;
        written = write(pBuf, bufSize);
        if (written > 0)
        {
            ret += written;
            if (written < bufSize)
                break;
        }
        else if (!written || ret)
            break;
        else
            return LS_FAIL;
    }
    if (finished)
        *finished = (vect == pEnd);
    return ret;
}


int SslConnection::wpending()
{
    return 0;
}


int SslConnection::flush()
{
    DEBUG_MESSAGE("[SSL: %p] flush\n", this);
    return 1; // Nothing to flush
}


int SslConnection::shutdown(int bidirectional)
{
    assert(m_ssl);
    m_flag = 0;
    if (m_iStatus == ACCEPTING)
    {
        ERR_clear_error();
        m_iStatus = DISCONNECTED;
        return 0;
    }
    if (m_iStatus != DISCONNECTED)
    {
        m_iWant = 0;
        SSL_set_shutdown(m_ssl, SSL_RECEIVED_SHUTDOWN);
        //SSL_set_quiet_shutdown( m_ssl, !bidirectional );
        int ret = SSL_shutdown(m_ssl);
        if ((ret) ||
            ((ret == 0) && (!bidirectional)))
        {
            m_iStatus = DISCONNECTED;
            return 0;
        }
        else
            m_iStatus = SHUTDOWN;
        ret = checkError(ret);
    }
    return 0;
}


void SslConnection::toAccept()
{
    DEBUG_MESSAGE("[SSL: %p] toAccept\n", this);
    m_iStatus = ACCEPTING;
    m_iWant = READ;
    SSL_set_accept_state(m_ssl);
}


int SslConnection::accept()
{
    int ret;
    DEBUG_MESSAGE("[SSL: %p] accept\n", this);
    assert(m_ssl);
    if (m_iStatus != ACCEPTING) {
        DEBUG_MESSAGE("[SSL: %p] In accept but not in ACCEPTING status\n",
                      this);
        return -1;
    }
    ret = SSL_do_handshake(m_ssl);
    DEBUG_MESSAGE("[SSL: %p] SSL_accept rc: %d\n", this, ret);
    if (ret == 1)
    {
        DEBUG_MESSAGE("[SSL: %p] SSL_accept worked - move to connected"
                        " status\n", this);
        m_iStatus = CONNECTED;
        m_iWant = READ;
    }
    else
    {
        ret = checkError(ret);
    }
    return ret;
}



int SslConnection::checkError(int ret)
{
    int err = SSL_get_error(m_ssl, ret);
#ifdef DEBUGGING
    char errorString1[1024];
    char errorString2[1024];
    DEBUG_MESSAGE("[SSL: %p] checkError returned %d, first error: %s, "
                  "last error: %s\n", this, err,
                  ERR_error_string(ERR_peek_error(), errorString1),
                  ERR_error_string(ERR_peek_last_error(), errorString2));
#endif
    switch (err)
    {
    case SSL_ERROR_ZERO_RETURN:
        errno = ECONNRESET;
        break;
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
        if (err == SSL_ERROR_WANT_READ)
            m_iWant |= READ;
        else
            m_iWant |= WRITE;
        ERR_clear_error();
        return 0;
    case SSL_ERROR_WANT_X509_LOOKUP:
        LS_DBG_H("[SSLSNI] need to pause SSL accept to fetch cert.");
        m_iStatus = WAITINGCERT;
        ERR_clear_error();
        return 0; // This will trigger a setSSLAgain(), which will disable read/write.
    case SSL_ERROR_SSL:
        if (m_iWant == LAST_WRITE)
        {
            m_iWant |= WRITE;
            ERR_clear_error();
            return 0;
        }
        //fall through
    case SSL_ERROR_SYSCALL:
        {
            int ret = ERR_get_error();
            if (ret == 0)
            {
                errno = ECONNRESET;
                break;
            }
        }
    default:
        LS_DBG_H("SslError:%s\n", SslError(err).what());
        errno = EIO;
    }
    return -1;
}


int SslConnection::connect()
{
    assert(m_ssl);
    m_iStatus = CONNECTING;
    m_iWant = 0;
    int ret = SSL_connect(m_ssl);
    if (ret == 1)
    {
        m_iStatus = CONNECTED;
        return 1;
    }
    else
        return checkError(ret);
}


int SslConnection::tryagain()
{    
    DEBUG_MESSAGE("[SSL: %p] tryagain!\n", this);
    assert(m_ssl);
    switch (m_iStatus)
    {
    case CONNECTING:
        return connect();
    case ACCEPTING:
        return accept();
    case SHUTDOWN:
        return shutdown(1);
    }
    return 0;
}


int SslConnection::updateOnGotCert()
{
    assert(m_iStatus == WAITINGCERT);
    m_iStatus = GOTCERT;
    return 0;
}


X509 *SslConnection::getPeerCertificate() const
{   return SSL_get_peer_certificate(m_ssl);   }


long SslConnection::getVerifyResult() const
{   return SSL_get_verify_result(m_ssl);      }


int SslConnection::getVerifyMode() const
{   return SSL_get_verify_mode(m_ssl);        }


const char *SslConnection::getCipherName() const
{   return SSL_get_cipher_name(m_ssl);    }


const SSL_CIPHER *SslConnection::getCurrentCipher() const
{   return SSL_get_current_cipher(m_ssl); }


SSL_SESSION *SslConnection::getSession() const
{   return SSL_get_session(m_ssl);        }


int SslConnection::setSession(SSL_SESSION *session) const
{   return SSL_set_session(m_ssl, session);     }


int SslConnection::isSessionReused() const
{   return SSL_session_reused(m_ssl);       }


const char *SslConnection::getVersion() const
{   return SSL_get_version(m_ssl);        }


#ifndef TEST_PGM
#define LS_ENABLE_SPDY
#endif
#ifdef LS_ENABLE_SPDY
static const char NPN_SPDY_PREFIX[] = { 's', 'p', 'd', 'y', '/' };
#endif
int SslConnection::getSpdyVersion()
{
    int v = 0;

    DEBUG_MESSAGE("[SSL: %p] getSpdyVersion\n", this);
    
#ifdef LS_ENABLE_SPDY
    unsigned int             len = 0;
    const unsigned char     *data = NULL;

#ifdef TLSEXT_TYPE_application_layer_protocol_negotiation
    SSL_get0_alpn_selected(m_ssl, &data, &len);
#endif

#ifdef TLSEXT_TYPE_next_proto_neg
    if (!data)
        SSL_get0_next_proto_negotiated(m_ssl, &data, &len);
#endif
    if (len > sizeof(NPN_SPDY_PREFIX) &&
        strncasecmp((const char *)data, NPN_SPDY_PREFIX,
                    sizeof(NPN_SPDY_PREFIX)) == 0)
    {
        v = data[ sizeof(NPN_SPDY_PREFIX) ] - '1';
        if ((v == 2) && (len >= 8) && (data[6] == '.') && (data[7] == '1'))
            v = 3;
        return v;
    }

    //h2: http2 version is 4
    if (len >= 2 && data[0] == 'h' && data[1] == '2')
        return 4;
#endif
    return v;
}


void SslConnection::initConnIdx()
{
    if (s_iConnIdx < 0)
        s_iConnIdx = SSL_get_ex_new_index(0, NULL, NULL, NULL, NULL);
}


int SslConnection::getCipherBits(const SSL_CIPHER *pCipher,
                                 int *algkeysize)
{
    return SSL_CIPHER_get_bits((SSL_CIPHER *)pCipher, algkeysize);
}


int SslConnection::isClientVerifyOptional(int i)
{   return i == SSL_VERIFY_PEER;    }


int SslConnection::isVerifyOk() const
{   return SSL_get_verify_result(m_ssl) == X509_V_OK;     }


int SslConnection::buildVerifyErrorString(char *pBuf, int len) const
{
    return snprintf(pBuf, len, "FAILED: %s", X509_verify_cert_error_string(
                        SSL_get_verify_result(m_ssl)));
}


int SslConnection::setTlsExtHostName(const char *pName)
{
    if (pName)
        return SSL_set_tlsext_host_name(m_ssl, pName);
    return  0;
}


const char *SslConnection::getTlsExtHostName()
{
    if (m_iStatus >= ACCEPTING)
        return SSL_get_servername(m_ssl, TLSEXT_NAMETYPE_host_name);
    return NULL;
}


int SslConnection::getEnv(HioCrypto::ENV id, char *&pValue, int bufLen)
{
    SSL_SESSION *pSession;
    X509 *pClientCert;
    const SSL_CIPHER *pCipher;
    int algkeysize;
    int keysize;
    int ret = 0;

    switch(id)
    {
    case CRYPTO_VERSION:
        pValue = (char *)getVersion();
        ret = strlen(pValue);
        break;
    case SESSION_ID:
        pSession = getSession();
        if (pSession)
        {
            unsigned int idLen;
            const unsigned char *pId;
            pId = SSL_SESSION_get_id(pSession, &idLen);
            ret = idLen * 2;
            if (ret > bufLen)
                ret = bufLen;
            StringTool::hexEncode((char *)pId, ret / 2, pValue);
        }
        break;
    case CLIENT_CERT:
        pClientCert = getPeerCertificate();
        if (pClientCert)
            ret = SslCert::PEMWriteCert(pClientCert, pValue, bufLen);
        break;
    case CIPHER:
        pValue = (char *)SSL_get_cipher_name(m_ssl);
        ret = strlen(pValue);
        break;
    case CIPHER_USEKEYSIZE:
    case CIPHER_ALGKEYSIZE:
        pCipher = getCurrentCipher();
        keysize = SSL_CIPHER_get_bits((SSL_CIPHER *)pCipher, &algkeysize);
        if (id == CIPHER_USEKEYSIZE)
            ret = snprintf(pValue, 20, "%d", keysize);
        else //LSI_VAR_SSL_CIPHER_ALGKEYSIZE
            ret = snprintf(pValue, 20, "%d", algkeysize);
        break;
    default:
        break;
    }
    return ret;
}


char* SslConnection::getRawBuffer(int *len)
{
    DEBUG_MESSAGE("[SSL: %p] getRawBuffer: len: %d\n", this,
                  m_bio.m_rbioBuffered);
    *len = m_bio.m_rbioBuffered;
    return m_bio.m_rbioBuf;
}


int SslConnection::cacheClientSession(SSL_SESSION* session, const char *pHost,
                                      int iHostLen)
{
    if (m_pSessCache)
    {
        m_pSessCache->saveSession(pHost, iHostLen, session);
        return 1; //take ownership of ssession
    }
    return 0;
}


void SslConnection::tryReuseCachedSession(const char *pHost, int iHostLen)
{
    if (!m_pSessCache)
        return;
    SSL_SESSION *session = m_pSessCache->getSession(pHost, iHostLen);
    if (session)
        SSL_set_session(m_ssl, session);
}


