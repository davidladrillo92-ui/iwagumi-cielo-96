#include "cielo_https_proxy.h"
#include <esp_https_server.h>
#include <esp_system.h>
#include <mbedtls/pk.h>
#include <mbedtls/ecp.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/x509.h>
#include <mbedtls/sha256.h>
#include <mbedtls/oid.h>
#include <lwip/sockets.h>
#include <lwip/inet.h>
#include <string.h>
#include <stdio.h>
namespace cielo96 {
namespace {
static httpd_handle_t tlsServer=nullptr;
static String tlsCertificate,tlsPrivateKey,tlsFingerprint;
static int randHardware(void*,unsigned char*out,size_t n){esp_fill_random(out,n);return 0;}
// Generar identidad criptografica unica, no incluir clave privada en compilacion.
static bool provisionTls(Preferences&prefs){
 tlsCertificate=prefs.getString("https_cert","");
 tlsPrivateKey=prefs.getString("https_key","");
 if(tlsCertificate.startsWith("-----BEGIN CERTIFICATE-----") &&
    tlsPrivateKey.startsWith("-----BEGIN EC PRIVATE KEY-----"))return true;
 mbedtls_pk_context pk;mbedtls_pk_init(&pk);
 mbedtls_x509write_cert crt;mbedtls_x509write_crt_init(&crt);
 mbedtls_mpi serial;mbedtls_mpi_init(&serial);
 bool ok=false;
 do {
  if(mbedtls_pk_setup(&pk,mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY))!=0)break;
  if(mbedtls_ecp_gen_key(MBEDTLS_ECP_DP_SECP256R1,mbedtls_pk_ec(pk),randHardware,nullptr)!=0)break;
  uint8_t randomSerial[16];esp_fill_random(randomSerial,sizeof randomSerial);randomSerial[0]&=0x7F;
  if(mbedtls_mpi_read_binary(&serial,randomSerial,sizeof randomSerial)!=0)break;
  mbedtls_x509write_crt_set_md_alg(&crt,MBEDTLS_MD_SHA256);
  mbedtls_x509write_crt_set_subject_key(&crt,&pk);
  mbedtls_x509write_crt_set_issuer_key(&crt,&pk);
  if(mbedtls_x509write_crt_set_subject_name(&crt,"CN=iwagumi-cielo96.local,O=IWAGUMI CIELO")!=0)break;
  if(mbedtls_x509write_crt_set_issuer_name(&crt,"CN=iwagumi-cielo96.local,O=IWAGUMI CIELO")!=0)break;
  if(mbedtls_x509write_crt_set_serial(&crt,&serial)!=0)break;
  if(mbedtls_x509write_crt_set_validity(&crt,"20260101000000","20400101000000")!=0)break;
  if(mbedtls_x509write_crt_set_basic_constraints(&crt,0,-1)!=0)break;
  if(mbedtls_x509write_crt_set_key_usage(&crt,MBEDTLS_X509_KU_DIGITAL_SIGNATURE)!=0)break;
  // subjectAltName dNSName = iwagumi-cielo96.local; la fecha y el nombre
  // se verifican en Safari SOLO despues de confiar explicitamente en el certificado.
  static constexpr char dns[]="iwagumi-cielo96.local";
  unsigned char san[4+sizeof(dns)-1];
  san[0]=0x30;san[1]=sizeof(dns)+1;san[2]=0x82;san[3]=sizeof(dns)-1;
  memcpy(san+4,dns,sizeof(dns)-1);
  if(mbedtls_x509write_crt_set_extension(&crt,MBEDTLS_OID_SUBJECT_ALT_NAME,
      MBEDTLS_OID_SIZE(MBEDTLS_OID_SUBJECT_ALT_NAME),0,san,sizeof(san))!=0)break;
  unsigned char keyPem[1024]{};
  unsigned char certPem[2048]{};
  if(mbedtls_pk_write_key_pem(&pk,keyPem,sizeof(keyPem))!=0)break;
  if(mbedtls_x509write_crt_pem(&crt,certPem,sizeof(certPem),randHardware,nullptr)!=0)break;
  tlsCertificate=String(reinterpret_cast<char*>(certPem));
  tlsPrivateKey=String(reinterpret_cast<char*>(keyPem));
  // No sobrescribir identidad si NVS esta llena; rechazar HTTPS si no se persiste.
  if(prefs.putString("https_key",tlsPrivateKey)!=tlsPrivateKey.length() ||
     prefs.putString("https_cert",tlsCertificate)!=tlsCertificate.length())break;
  ok=true;
 }while(false);
 mbedtls_mpi_free(&serial);mbedtls_x509write_crt_free(&crt);mbedtls_pk_free(&pk);
 if(!ok){tlsCertificate="";tlsPrivateKey="";}
 return ok;
}
static bool sendAll(int fd,const uint8_t*buf,size_t len){
 while(len){const int n=send(fd,buf,len,0);if(n<=0)return false;buf+=n;len-=n;}return true;
}
static bool sendAll(int fd,const String&v){return sendAll(fd,(const uint8_t*)v.c_str(),v.length());}
static String headerValue(httpd_req_t*r,const char*name){
 const size_t need=httpd_req_get_hdr_value_len(r,name);
 if(!need||need>4095)return "";
 char* buf=(char*)malloc(need+1);if(!buf)return "";
 String out;
 if(httpd_req_get_hdr_value_str(r,name,buf,need+1)==ESP_OK)out=buf;
 free(buf);return out;
}
static esp_err_t gateway(httpd_req_t*req){
 // Solicitudes reenviadas UNICAMENTE al HTTP interno 127.0.0.1.
 // Limitar las llamadas a su puerto 80 desde otros hosts: el servidor web
 // comprueba que la IP origen sea localhost antes de aceptar mutaciones.
 if(req->content_len>0x340000){httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"Carga demasiado grande");return ESP_FAIL;}
 const int sock=socket(AF_INET,SOCK_STREAM,IPPROTO_IP);
 if(sock<0){httpd_resp_send_500(req);return ESP_FAIL;}
 struct timeval tv={125,0};setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));
 sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_port=htons(80);addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
 if(connect(sock,(sockaddr*)&addr,sizeof(addr))!=0){close(sock);httpd_resp_send_500(req);return ESP_FAIL;}
 const char*method=req->method==HTTP_POST?"POST":"GET";
 String url=String(req->uri);
 if(url.length()>180||url.indexOf('\r')>=0||url.indexOf('\n')>=0){close(sock);httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"URI invalida");return ESP_FAIL;}
 String head=String(method)+" "+url+" HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n";
 const char*forward[]={"Content-Type","X-Cielo-Key","X-Cielo-Sha256","X-Cielo-Signature"};
 for(const char*name:forward){String v=headerValue(req,name);if(v.length() && v.indexOf('\r')<0&&v.indexOf('\n')<0)head+=String(name)+": "+v+"\r\n";}
 head+="Content-Length: "+String((unsigned)req->content_len)+"\r\n\r\n";
 if(!sendAll(sock,head)){close(sock);httpd_resp_send_500(req);return ESP_FAIL;}
 uint8_t buffer[1536];size_t left=req->content_len;
 while(left){
  int n=httpd_req_recv(req,(char*)buffer,left>sizeof(buffer)?sizeof(buffer):left);
  if(n<=0||!sendAll(sock,buffer,n)){close(sock);httpd_resp_send_500(req);return ESP_FAIL;}
  left-=n;
 }
 // Leer SOLO las cabeceras de respuesta en memoria, nunca cargar un HTML de 100KB.
 String headers="";headers.reserve(4096);int headEnd=-1;
 while(headers.length()<6144 && headEnd<0){
  char c;int n=recv(sock,&c,1,0);if(n<=0)break;
  headers+=c;headEnd=headers.indexOf("\r\n\r\n");
 }
 if(headEnd<0){close(sock);httpd_resp_send_500(req);return ESP_FAIL;}
 const int statusEnd=headers.indexOf("\r\n");
 if(statusEnd<0||!headers.startsWith("HTTP/")){close(sock);httpd_resp_send_500(req);return ESP_FAIL;}
 String status=headers.substring(headers.indexOf(' ')+1,statusEnd);httpd_resp_set_status(req,status.c_str());
 auto backendHeader=[&headers](const char*key)->String{
  const String needle=String("\r\n")+key+":";
  int pos=headers.indexOf(needle);if(pos<0)return "";
  pos+=needle.length();while(headers[pos]==' ')pos++;
  int end=headers.indexOf("\r\n",pos);return end<0?"":headers.substring(pos,end);
 };
 String contentType=backendHeader("Content-Type");
 if(contentType.length())httpd_resp_set_type(req,contentType.c_str());
 String disposition=backendHeader("Content-Disposition");
 if(disposition.length())httpd_resp_set_hdr(req,"Content-Disposition",disposition.c_str());
 httpd_resp_set_hdr(req,"Cache-Control","no-store");
 bool success=true;
 // HTTP del servidor Arduino con longitud conocida: transmitir fragmentos TLS.
 // La respuesta del proxy usa Transfer-Encoding: chunked emitido por IDF.
 for(;;){
  int n=recv(sock,buffer,sizeof(buffer),0);
  if(n==0)break;
  if(n<0){success=false;break;}
  if(httpd_resp_send_chunk(req,(const char*)buffer,n)!=ESP_OK){success=false;break;}
 }
 close(sock);
 if(success)httpd_resp_send_chunk(req,nullptr,0);
 return success?ESP_OK:ESP_FAIL;
}
} // namespace
bool https_begin(Preferences&prefs){
 if(tlsServer)return true;
 if(!provisionTls(prefs)){Serial.println("CIELO HTTPS: fallo provision NVS, escrituras bloqueadas");return false;}
 // Huella de certificado DER para confirmar identidad por canal fisico USB.
 mbedtls_x509_crt crt;mbedtls_x509_crt_init(&crt);
 if(mbedtls_x509_crt_parse(&crt,(const unsigned char*)tlsCertificate.c_str(),tlsCertificate.length()+1)==0){
  uint8_t hash[32];mbedtls_sha256_ret(crt.raw.p,crt.raw.len,hash,0);
  const char* hex="0123456789ABCDEF";
  for(uint8_t b:hash){tlsFingerprint+=hex[b>>4];tlsFingerprint+=hex[b&15];}
 }
 mbedtls_x509_crt_free(&crt);
 httpd_ssl_config_t cfg=HTTPD_SSL_CONFIG_DEFAULT();
 cfg.httpd.stack_size=12288;
 cfg.httpd.max_uri_handlers=2;
 cfg.httpd.uri_match_fn=httpd_uri_match_wildcard;
 cfg.httpd.recv_wait_timeout=120;
 cfg.httpd.send_wait_timeout=120;
 cfg.cacert_pem=(const uint8_t*)tlsCertificate.c_str();cfg.cacert_len=tlsCertificate.length()+1;
 cfg.prvtkey_pem=(const uint8_t*)tlsPrivateKey.c_str();cfg.prvtkey_len=tlsPrivateKey.length()+1;
 if(httpd_ssl_start(&tlsServer,&cfg)!=ESP_OK){tlsServer=nullptr;return false;}
 httpd_uri_t get{};get.uri="/*";get.method=HTTP_GET;get.handler=gateway;
 httpd_uri_t post{};post.uri="/*";post.method=HTTP_POST;post.handler=gateway;
 if(httpd_register_uri_handler(tlsServer,&get)!=ESP_OK||httpd_register_uri_handler(tlsServer,&post)!=ESP_OK){
  httpd_ssl_stop(tlsServer);tlsServer=nullptr;return false;
 }
 Serial.printf("CIELO HTTPS: https://iwagumi-cielo96.local , fingerprint SHA256 %s\n",tlsFingerprint.c_str());
 return true;
}
bool https_ready(){return tlsServer!=nullptr;}
String https_cert_fingerprint(){return tlsFingerprint;}
}
