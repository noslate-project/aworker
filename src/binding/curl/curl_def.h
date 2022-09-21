#ifndef SRC_BINDING_CURL_CURL_DEF_H_
#define SRC_BINDING_CURL_CURL_DEF_H_

/**
 * @{link CURLcode}
 */
#define CURL_CODE_ALL(V)                                                       \
  V(CURLE_OK)                                                                  \
  V(CURLE_UNSUPPORTED_PROTOCOL)                                                \
  V(CURLE_FAILED_INIT)                                                         \
  V(CURLE_URL_MALFORMAT)                                                       \
  V(CURLE_NOT_BUILT_IN)                                                        \
  V(CURLE_COULDNT_RESOLVE_PROXY)                                               \
  V(CURLE_COULDNT_RESOLVE_HOST)                                                \
  V(CURLE_COULDNT_CONNECT)                                                     \
  V(CURLE_WEIRD_SERVER_REPLY)                                                  \
  V(CURLE_REMOTE_ACCESS_DENIED)                                                \
  V(CURLE_FTP_ACCEPT_FAILED)                                                   \
  V(CURLE_FTP_WEIRD_PASS_REPLY)                                                \
  V(CURLE_FTP_ACCEPT_TIMEOUT)                                                  \
  V(CURLE_FTP_WEIRD_PASV_REPLY)                                                \
  V(CURLE_FTP_WEIRD_227_FORMAT)                                                \
  V(CURLE_FTP_CANT_GET_HOST)                                                   \
  V(CURLE_HTTP2)                                                               \
  V(CURLE_FTP_COULDNT_SET_TYPE)                                                \
  V(CURLE_PARTIAL_FILE)                                                        \
  V(CURLE_FTP_COULDNT_RETR_FILE)                                               \
  V(CURLE_OBSOLETE20)                                                          \
  V(CURLE_QUOTE_ERROR)                                                         \
  V(CURLE_HTTP_RETURNED_ERROR)                                                 \
  V(CURLE_WRITE_ERROR)                                                         \
  V(CURLE_OBSOLETE24)                                                          \
  V(CURLE_UPLOAD_FAILED)                                                       \
  V(CURLE_READ_ERROR)                                                          \
  V(CURLE_OUT_OF_MEMORY)                                                       \
  V(CURLE_OPERATION_TIMEDOUT)                                                  \
  V(CURLE_OBSOLETE29)                                                          \
  V(CURLE_FTP_PORT_FAILED)                                                     \
  V(CURLE_FTP_COULDNT_USE_REST)                                                \
  V(CURLE_OBSOLETE32)                                                          \
  V(CURLE_RANGE_ERROR)                                                         \
  V(CURLE_HTTP_POST_ERROR)                                                     \
  V(CURLE_SSL_CONNECT_ERROR)                                                   \
  V(CURLE_BAD_DOWNLOAD_RESUME)                                                 \
  V(CURLE_FILE_COULDNT_READ_FILE)                                              \
  V(CURLE_LDAP_CANNOT_BIND)                                                    \
  V(CURLE_LDAP_SEARCH_FAILED)                                                  \
  V(CURLE_OBSOLETE40)                                                          \
  V(CURLE_FUNCTION_NOT_FOUND)                                                  \
  V(CURLE_ABORTED_BY_CALLBACK)                                                 \
  V(CURLE_BAD_FUNCTION_ARGUMENT)                                               \
  V(CURLE_OBSOLETE44)                                                          \
  V(CURLE_INTERFACE_FAILED)                                                    \
  V(CURLE_OBSOLETE46)                                                          \
  V(CURLE_TOO_MANY_REDIRECTS)                                                  \
  V(CURLE_UNKNOWN_OPTION)                                                      \
  V(CURLE_TELNET_OPTION_SYNTAX)                                                \
  V(CURLE_OBSOLETE50)                                                          \
  V(CURLE_OBSOLETE51)                                                          \
  V(CURLE_GOT_NOTHING)                                                         \
  V(CURLE_SSL_ENGINE_NOTFOUND)                                                 \
  V(CURLE_SSL_ENGINE_SETFAILED)                                                \
  V(CURLE_SEND_ERROR)                                                          \
  V(CURLE_RECV_ERROR)                                                          \
  V(CURLE_OBSOLETE57)                                                          \
  V(CURLE_SSL_CERTPROBLEM)                                                     \
  V(CURLE_SSL_CIPHER)                                                          \
  V(CURLE_PEER_FAILED_VERIFICATION)                                            \
  V(CURLE_BAD_CONTENT_ENCODING)                                                \
  V(CURLE_LDAP_INVALID_URL)                                                    \
  V(CURLE_FILESIZE_EXCEEDED)                                                   \
  V(CURLE_USE_SSL_FAILED)                                                      \
  V(CURLE_SEND_FAIL_REWIND)                                                    \
  V(CURLE_SSL_ENGINE_INITFAILED)                                               \
  V(CURLE_LOGIN_DENIED)                                                        \
  V(CURLE_TFTP_NOTFOUND)                                                       \
  V(CURLE_TFTP_PERM)                                                           \
  V(CURLE_REMOTE_DISK_FULL)                                                    \
  V(CURLE_TFTP_ILLEGAL)                                                        \
  V(CURLE_TFTP_UNKNOWNID)                                                      \
  V(CURLE_REMOTE_FILE_EXISTS)                                                  \
  V(CURLE_TFTP_NOSUCHUSER)                                                     \
  V(CURLE_CONV_FAILED)                                                         \
  V(CURLE_CONV_REQD)                                                           \
  V(CURLE_SSL_CACERT_BADFILE)                                                  \
  V(CURLE_REMOTE_FILE_NOT_FOUND)                                               \
  V(CURLE_SSH)                                                                 \
  V(CURLE_SSL_SHUTDOWN_FAILED)                                                 \
  V(CURLE_AGAIN)                                                               \
  V(CURLE_SSL_CRL_BADFILE)                                                     \
  V(CURLE_SSL_ISSUER_ERROR)                                                    \
  V(CURLE_FTP_PRET_FAILED)                                                     \
  V(CURLE_RTSP_CSEQ_ERROR)                                                     \
  V(CURLE_RTSP_SESSION_ERROR)                                                  \
  V(CURLE_FTP_BAD_FILE_LIST)                                                   \
  V(CURLE_CHUNK_FAILED)                                                        \
  V(CURLE_NO_CONNECTION_AVAILABLE)                                             \
  V(CURLE_SSL_PINNEDPUBKEYNOTMATCH)                                            \
  V(CURLE_SSL_INVALIDCERTSTATUS)                                               \
  V(CURLE_HTTP2_STREAM)                                                        \
  V(CURLE_RECURSIVE_API_CALL)                                                  \
  V(CURLE_AUTH_ERROR)                                                          \
  V(CURLE_HTTP3)

/**
 * See https://curl.se/libcurl/c/curl_easy_setopt.html
 */
#define CURL_OPTS_INTEGER(V)                                                   \
  V(CURLOPT_ACCEPTTIMEOUT_MS)                                                  \
  V(CURLOPT_BUFFERSIZE)                                                        \
  V(CURLOPT_CONNECTTIMEOUT_MS)                                                 \
  V(CURLOPT_CONNECT_ONLY)                                                      \
  V(CURLOPT_COOKIESESSION)                                                     \
  V(CURLOPT_DNS_CACHE_TIMEOUT)                                                 \
  V(CURLOPT_DNS_SHUFFLE_ADDRESSES)                                             \
  V(CURLOPT_EXPECT_100_TIMEOUT_MS)                                             \
  V(CURLOPT_FOLLOWLOCATION)                                                    \
  V(CURLOPT_FORBID_REUSE)                                                      \
  V(CURLOPT_FRESH_CONNECT)                                                     \
  V(CURLOPT_HTTPAUTH)                                                          \
  V(CURLOPT_HTTPGET)                                                           \
  V(CURLOPT_HTTP_CONTENT_DECODING)                                             \
  V(CURLOPT_HTTP_TRANSFER_DECODING)                                            \
  V(CURLOPT_IGNORE_CONTENT_LENGTH)                                             \
  V(CURLOPT_IPRESOLVE)                                                         \
  V(CURLOPT_MAXAGE_CONN)                                                       \
  V(CURLOPT_MAXCONNECTS)                                                       \
  V(CURLOPT_MAXREDIRS)                                                         \
  V(CURLOPT_NOBODY)                                                            \
  V(CURLOPT_NOPROGRESS)                                                        \
  V(CURLOPT_NOSIGNAL)                                                          \
  V(CURLOPT_PATH_AS_IS)                                                        \
  V(CURLOPT_POST)                                                              \
  V(CURLOPT_POSTFIELDSIZE)                                                     \
  V(CURLOPT_POSTREDIR)                                                         \
  V(CURLOPT_PROTOCOLS)                                                         \
  V(CURLOPT_PUT)                                                               \
  V(CURLOPT_SSL_OPTIONS)                                                       \
  V(CURLOPT_SSL_ENABLE_ALPN)                                                   \
  V(CURLOPT_SSL_ENABLE_NPN)                                                    \
  V(CURLOPT_SSL_SESSIONID_CACHE)                                               \
  V(CURLOPT_SSL_VERIFYHOST)                                                    \
  V(CURLOPT_SSL_VERIFYPEER)                                                    \
  V(CURLOPT_SSL_VERIFYSTATUS)                                                  \
  V(CURLOPT_SSLVERSION)                                                        \
  V(CURLOPT_SUPPRESS_CONNECT_HEADERS)                                          \
  V(CURLOPT_TCP_FASTOPEN)                                                      \
  V(CURLOPT_TCP_KEEPALIVE)                                                     \
  V(CURLOPT_TCP_KEEPIDLE)                                                      \
  V(CURLOPT_TCP_KEEPINTVL)                                                     \
  V(CURLOPT_TCP_NODELAY)                                                       \
  V(CURLOPT_TIMECONDITION)                                                     \
  V(CURLOPT_TIMEOUT_MS)                                                        \
  V(CURLOPT_TIMEVALUE)                                                         \
  V(CURLOPT_TRANSFER_ENCODING)                                                 \
  V(CURLOPT_UNRESTRICTED_AUTH)                                                 \
  V(CURLOPT_UPLOAD)                                                            \
  V(CURLOPT_UPLOAD_BUFFERSIZE)                                                 \
  V(CURLOPT_UPKEEP_INTERVAL_MS)                                                \
  V(CURLOPT_VERBOSE)

/**
 * See https://curl.se/libcurl/c/curl_easy_setopt.html
 */
#define CURL_OPTS_STRING(V)                                                    \
  V(CURLOPT_ACCEPT_ENCODING)                                                   \
  V(CURLOPT_COOKIE)                                                            \
  V(CURLOPT_CUSTOMREQUEST)                                                     \
  V(CURLOPT_DNS_INTERFACE)                                                     \
  V(CURLOPT_DNS_LOCAL_IP4)                                                     \
  V(CURLOPT_DNS_LOCAL_IP6)                                                     \
  V(CURLOPT_DNS_SERVERS)                                                       \
  V(CURLOPT_INTERFACE)                                                         \
  V(CURLOPT_PASSWORD)                                                          \
  V(CURLOPT_POSTFIELDS)                                                        \
  V(CURLOPT_REFERER)                                                           \
  V(CURLOPT_SERVICE_NAME)                                                      \
  V(CURLOPT_SSLCERT)                                                           \
  V(CURLOPT_SSLCERTTYPE)                                                       \
  V(CURLOPT_SSLENGINE)                                                         \
  V(CURLOPT_SSLKEY)                                                            \
  V(CURLOPT_SSLKEYTYPE)                                                        \
  V(CURLOPT_SSL_CIPHER_LIST)                                                   \
  V(CURLOPT_UNIX_SOCKET_PATH)                                                  \
  V(CURLOPT_URL)                                                               \
  V(CURLOPT_USERAGENT)                                                         \
  V(CURLOPT_USERNAME)

/**
 * See https://curl.se/libcurl/c/curl_easy_setopt.html
 */
#define CURL_OPTS_LIST(V)                                                      \
  V(CURLOPT_HTTPHEADER)                                                        \
  V(CURLOPT_HTTPPOST)                                                          \
  V(CURLOPT_RESOLVE)

/**
 * See https://curl.se/libcurl/c/CURLOPT_IPRESOLVE.html
 */
#define CURL_IPRESOLVE_OPTS(V)                                                 \
  V(CURL_IPRESOLVE_WHATEVER)                                                   \
  V(CURL_IPRESOLVE_V4)                                                         \
  V(CURL_IPRESOLVE_V6)

/**
 * See https://curl.se/libcurl/c/CURLOPT_SSL_OPTIONS.html
 */
#define CURL_SSL_OPTS(V)                                                       \
  V(CURLSSLOPT_ALLOW_BEAST)                                                    \
  V(CURLSSLOPT_NO_REVOKE)                                                      \
  V(CURLSSLOPT_NO_PARTIALCHAIN)

/**
 * https://curl.se/libcurl/c/curl_easy_pause.html
 */
#define CURL_PAUSE_OPTS(V)                                                     \
  V(CURLPAUSE_RECV)                                                            \
  V(CURLPAUSE_SEND)                                                            \
  V(CURLPAUSE_ALL)                                                             \
  V(CURLPAUSE_CONT)

#define CURL_READFUNC_FLAGS(V)                                                 \
  V(CURL_READFUNC_PAUSE)                                                       \
  V(CURL_READFUNC_ABORT)

#endif  // SRC_BINDING_CURL_CURL_DEF_H_
