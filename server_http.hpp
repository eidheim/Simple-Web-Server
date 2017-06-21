#ifndef SERVER_HTTP_HPP
#define	SERVER_HTTP_HPP

#include <map>
#include <unordered_map>
#include <thread>
#include <functional>
#include <iostream>
#include <sstream>
#include <iomanip>

#ifdef USE_STANDALONE_ASIO
#include <asio.hpp>
#include <type_traits>
namespace SimpleWeb {
    using error_code = std::error_code;
    using errc = std::errc;
    namespace make_error_code = std;
}
#else
#include <boost/asio.hpp>
namespace SimpleWeb {
    namespace asio = boost::asio;
    using error_code = boost::system::error_code;
    namespace errc = boost::system::errc;
    namespace make_error_code = boost::system::errc;
}
#endif

# ifndef CASE_INSENSITIVE_EQUAL_AND_HASH
# define CASE_INSENSITIVE_EQUAL_AND_HASH
namespace SimpleWeb {
    inline bool case_insensitive_equal(const std::string &str1, const std::string &str2) {
        return str1.size() == str2.size() &&
               std::equal(str1.begin(), str1.end(), str2.begin(), [](char a, char b) {
                   return tolower(a) == tolower(b);
               });
    }
    class CaseInsensitiveEqual {
    public:
        bool operator()(const std::string &str1, const std::string &str2) const {
            return case_insensitive_equal(str1, str2);
        }
    };
    // Based on https://stackoverflow.com/questions/2590677/how-do-i-combine-hash-values-in-c0x/2595226#2595226
    class CaseInsensitiveHash {
    public:
        size_t operator()(const std::string &str) const {
            size_t h = 0;
            std::hash<int> hash;
            for (auto c : str)
                h ^= hash(tolower(c)) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };

	// Based on https://stackoverflow.com/questions/154536/encode-decode-urls-in-c
	inline std::string url_encode(const std::string & str) {
		std::string new_str = "";
		char c;
		int ic;
		const char* chars = str.c_str();
		char bufHex[10];
		int len = strlen(chars);

		for (int i = 0; i<len; i++) {
			c = chars[i];
			ic = c;
			// uncomment this if you want to encode spaces with +
			/*if (c==' ') new_str += '+';
			else */if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') new_str += c;
			else {
				sprintf(bufHex, "%X", c);
				if (ic < 16)
					new_str += "%0";
				else
					new_str += "%";
				new_str += bufHex;
			}
		}
		return new_str;
	}

	inline std::string url_decode(const std::string & str) {
		std::string ret;
		char ch;
		int i, ii, len = str.length();

		for (i = 0; i < len; i++) {
			if (str[i] != '%') {
				if (str[i] == '+')
					ret += ' ';
				else
					ret += str[i];
			}
			else {
				sscanf(str.substr(i + 1, 2).c_str(), "%x", &ii);
				ch = static_cast<char>(ii);
				ret += ch;
				i = i + 2;
			}
		}
		return ret;
	}
}
# endif


// Late 2017 TODO: remove the following checks and always use std::regex
#ifdef USE_BOOST_REGEX
#include <boost/regex.hpp>
namespace SimpleWeb {
    namespace regex = boost;
}
#else
#include <regex>
namespace SimpleWeb {
    namespace regex = std;
}
#endif

// TODO when switching to c++14, use [[deprecated]] instead
#ifndef DEPRECATED
#ifdef __GNUC__
#define DEPRECATED __attribute__((deprecated))
#elif defined(_MSC_VER)
#define DEPRECATED __declspec(deprecated)
#else
#define DEPRECATED
#endif
#endif

namespace SimpleWeb {
    template <class socket_type>
    class Server;
    
    template <class socket_type>
    class ServerBase {
    public:
        virtual ~ServerBase() {}

        class Response : public std::ostream {
            friend class ServerBase<socket_type>;

            asio::streambuf streambuf;

            std::shared_ptr<socket_type> socket;

            Response(const std::shared_ptr<socket_type> &socket): std::ostream(&streambuf), socket(socket) {}

			int status = 200;
			

        public:
			std::map<int, std::string> http_status_codes = {
				{ 100,"Continue" },
				{ 101,"Switching Protocols" },
				{ 200,"OK" },
				{ 201,"Created" },
				{ 202,"Accepted" },
				{ 203,"Non-Authoritative Information" },
				{ 204,"No Content" },
				{ 205,"Reset Content" },
				{ 206,"Partial Content" },
				{ 300,"Multiple Choices" },
				{ 301,"Moved Permanently" },
				{ 302,"Found" },
				{ 303,"See Other" },
				{ 304,"Not Modified" },
				{ 305,"Use Proxy" },
				{ 307,"Temporary Redirect" },
				{ 400,"Bad Request" },
				{ 401,"Unauthorized" },
				{ 402,"Payment Required" },
				{ 403,"Forbidden" },
				{ 404,"Not Found" },
				{ 405,"Method Not Allowed" },
				{ 406,"Not Acceptable" },
				{ 407,"Proxy Authentication Required" },
				{ 408,"Request Timeout" },
				{ 409,"Conflict" },
				{ 410,"Gone" },
				{ 411,"Length Required" },
				{ 412,"Precondition Failed" },
				{ 413,"Payload Too Large" },
				{ 414,"URI Too Long" },
				{ 415,"Unsupported Media Type" },
				{ 416,"Range Not Satisfiable" },
				{ 417,"Expectation Failed" },
				{ 426,"Upgrade Required" },
				{ 500,"Internal Server Error" },
				{ 501,"Not Implemented" },
				{ 502,"Bad Gateway" },
				{ 503,"Service Unavailable" },
				{ 504,"Gateway Timeout" },
				{ 505,"HTTP Version Not Supported" }
			};

			std::map<std::string, std::string> http_mime_types = {
				{ ".3dm","x-world/x-3dmf" },
				{ ".3dmf","x-world/x-3dmf" },
				{ ".a","application/octet-stream" },
				{ ".aab","application/x-authorware-bin" },
				{ ".aam","application/x-authorware-map" },
				{ ".aas","application/x-authorware-seg" },
				{ ".abc","text/vnd.abc" },
				{ ".acgi","text/html" },
				{ ".afl","video/animaflex" },
				{ ".ai","application/postscript" },
				{ ".aif","audio/aiff" },
				{ ".aif","audio/x-aiff" },
				{ ".aifc","audio/aiff" },
				{ ".aifc","audio/x-aiff" },
				{ ".aiff","audio/aiff" },
				{ ".aiff","audio/x-aiff" },
				{ ".aim","application/x-aim" },
				{ ".aip","text/x-audiosoft-intra" },
				{ ".ani","application/x-navi-animation" },
				{ ".aos","application/x-nokia-9000-communicator-add-on-software" },
				{ ".aps","application/mime" },
				{ ".arc","application/octet-stream" },
				{ ".arj","application/arj" },
				{ ".arj","application/octet-stream" },
				{ ".art","image/x-jg" },
				{ ".asf","video/x-ms-asf" },
				{ ".asm","text/x-asm" },
				{ ".asp","text/asp" },
				{ ".asx","application/x-mplayer2" },
				{ ".asx","video/x-ms-asf" },
				{ ".asx","video/x-ms-asf-plugin" },
				{ ".au","audio/basic" },
				{ ".au","audio/x-au" },
				{ ".avi","application/x-troff-msvideo" },
				{ ".avi","video/avi" },
				{ ".avi","video/msvideo" },
				{ ".avi","video/x-msvideo" },
				{ ".avs","video/avs-video" },
				{ ".bcpio","application/x-bcpio" },
				{ ".bin","application/mac-binary" },
				{ ".bin","application/macbinary" },
				{ ".bin","application/octet-stream" },
				{ ".bin","application/x-binary" },
				{ ".bin","application/x-macbinary" },
				{ ".bm","image/bmp" },
				{ ".bmp","image/bmp" },
				{ ".bmp","image/x-windows-bmp" },
				{ ".boo","application/book" },
				{ ".book","application/book" },
				{ ".boz","application/x-bzip2" },
				{ ".bsh","application/x-bsh" },
				{ ".bz","application/x-bzip" },
				{ ".bz2","application/x-bzip2" },
				{ ".c","text/plain" },
				{ ".c","text/x-c" },
				{ ".c++","text/plain" },
				{ ".cat","application/vnd.ms-pki.seccat" },
				{ ".cc","text/plain" },
				{ ".cc","text/x-c" },
				{ ".ccad","application/clariscad" },
				{ ".cco","application/x-cocoa" },
				{ ".cdf","application/cdf" },
				{ ".cdf","application/x-cdf" },
				{ ".cdf","application/x-netcdf" },
				{ ".cer","application/pkix-cert" },
				{ ".cer","application/x-x509-ca-cert" },
				{ ".cha","application/x-chat" },
				{ ".chat","application/x-chat" },
				{ ".class","application/java" },
				{ ".class","application/java-byte-code" },
				{ ".class","application/x-java-class" },
				{ ".com","application/octet-stream" },
				{ ".com","text/plain" },
				{ ".conf","text/plain" },
				{ ".cpio","application/x-cpio" },
				{ ".cpp","text/x-c" },
				{ ".cpt","application/mac-compactpro" },
				{ ".cpt","application/x-compactpro" },
				{ ".cpt","application/x-cpt" },
				{ ".crl","application/pkcs-crl" },
				{ ".crl","application/pkix-crl" },
				{ ".crt","application/pkix-cert" },
				{ ".crt","application/x-x509-ca-cert" },
				{ ".crt","application/x-x509-user-cert" },
				{ ".csh","application/x-csh" },
				{ ".css","text/css" },
				{ ".cxx","text/plain" },
				{ ".dcr","application/x-director" },
				{ ".deepv","application/x-deepv" },
				{ ".def","text/plain" },
				{ ".der","application/x-x509-ca-cert" },
				{ ".dif","video/x-dv" },
				{ ".dir","application/x-director" },
				{ ".dl","video/dl" },
				{ ".dl","video/x-dl" },
				{ ".doc","application/msword" },
				{ ".dot","application/msword" },
				{ ".dp","application/commonground" },
				{ ".drw","application/drafting" },
				{ ".dump","application/octet-stream" },
				{ ".dv","video/x-dv" },
				{ ".dvi","application/x-dvi" },
				{ ".dwf","drawing/x-dwf (old)" },
				{ ".dwf","model/vnd.dwf" },
				{ ".dwg","application/acad" },
				{ ".dwg","image/vnd.dwg" },
				{ ".dwg","image/x-dwg" },
				{ ".dxf","application/dxf" },
				{ ".dxf","image/vnd.dwg" },
				{ ".dxf","image/x-dwg" },
				{ ".dxr","application/x-director" },
				{ ".el","text/x-script.elisp" },
				{ ".elc","application/x-bytecode.elisp" },
				{ ".elc","application/x-elc" },
				{ ".env","application/x-envoy" },
				{ ".eps","application/postscript" },
				{ ".es","application/x-esrehber" },
				{ ".etx","text/x-setext" },
				{ ".evy","application/envoy" },
				{ ".evy","application/x-envoy" },
				{ ".exe","application/octet-stream" },
				{ ".f","text/plain" },
				{ ".f","text/x-fortran" },
				{ ".f77","text/x-fortran" },
				{ ".f90","text/plain" },
				{ ".f90","text/x-fortran" },
				{ ".fdf","application/vnd.fdf" },
				{ ".fif","application/fractals" },
				{ ".fif","image/fif" },
				{ ".fli","video/fli" },
				{ ".fli","video/x-fli" },
				{ ".flo","image/florian" },
				{ ".flx","text/vnd.fmi.flexstor" },
				{ ".fmf","video/x-atomic3d-feature" },
				{ ".for","text/plain" },
				{ ".for","text/x-fortran" },
				{ ".fpx","image/vnd.fpx" },
				{ ".fpx","image/vnd.net-fpx" },
				{ ".frl","application/freeloader" },
				{ ".funk","audio/make" },
				{ ".g","text/plain" },
				{ ".g3","image/g3fax" },
				{ ".gif","image/gif" },
				{ ".gl","video/gl" },
				{ ".gl","video/x-gl" },
				{ ".gsd","audio/x-gsm" },
				{ ".gsm","audio/x-gsm" },
				{ ".gsp","application/x-gsp" },
				{ ".gss","application/x-gss" },
				{ ".gtar","application/x-gtar" },
				{ ".gz","application/x-compressed" },
				{ ".gz","application/x-gzip" },
				{ ".gzip","application/x-gzip" },
				{ ".gzip","multipart/x-gzip" },
				{ ".h","text/plain" },
				{ ".h","text/x-h" },
				{ ".hdf","application/x-hdf" },
				{ ".help","application/x-helpfile" },
				{ ".hgl","application/vnd.hp-hpgl" },
				{ ".hh","text/plain" },
				{ ".hh","text/x-h" },
				{ ".hlb","text/x-script" },
				{ ".hlp","application/hlp" },
				{ ".hlp","application/x-helpfile" },
				{ ".hlp","application/x-winhelp" },
				{ ".hpg","application/vnd.hp-hpgl" },
				{ ".hpgl","application/vnd.hp-hpgl" },
				{ ".hqx","application/binhex" },
				{ ".hqx","application/binhex4" },
				{ ".hqx","application/mac-binhex" },
				{ ".hqx","application/mac-binhex40" },
				{ ".hqx","application/x-binhex40" },
				{ ".hqx","application/x-mac-binhex40" },
				{ ".hta","application/hta" },
				{ ".htc","text/x-component" },
				{ ".htm","text/html" },
				{ ".html","text/html" },
				{ ".htmls","text/html" },
				{ ".htt","text/webviewhtml" },
				{ ".htx","text/html" },
				{ ".ice","x-conference/x-cooltalk" },
				{ ".ico","image/x-icon" },
				{ ".idc","text/plain" },
				{ ".ief","image/ief" },
				{ ".iefs","image/ief" },
				{ ".iges","application/iges" },
				{ ".iges","model/iges" },
				{ ".igs","application/iges" },
				{ ".igs","model/iges" },
				{ ".ima","application/x-ima" },
				{ ".imap","application/x-httpd-imap" },
				{ ".inf","application/inf" },
				{ ".ins","application/x-internett-signup" },
				{ ".ip","application/x-ip2" },
				{ ".isu","video/x-isvideo" },
				{ ".it","audio/it" },
				{ ".iv","application/x-inventor" },
				{ ".ivr","i-world/i-vrml" },
				{ ".ivy","application/x-livescreen" },
				{ ".jam","audio/x-jam" },
				{ ".jav","text/plain" },
				{ ".jav","text/x-java-source" },
				{ ".java","text/plain" },
				{ ".java","text/x-java-source" },
				{ ".jcm","application/x-java-commerce" },
				{ ".jfif","image/jpeg" },
				{ ".jfif","image/pjpeg" },
				{ ".jfif-tbnl","image/jpeg" },
				{ ".jpe","image/jpeg" },
				{ ".jpe","image/pjpeg" },
				{ ".jpeg","image/jpeg" },
				{ ".jpeg","image/pjpeg" },
				{ ".jpg","image/jpeg" },
				{ ".jpg","image/pjpeg" },
				{ ".jps","image/x-jps" },
				{ ".js","application/javascript" },
				{ ".js","application/ecmascript" },
				{ ".js","text/javascript" },
				{ ".js","text/ecmascript" },
				{ ".jut","image/jutvision" },
				{ ".kar","audio/midi" },
				{ ".kar","music/x-karaoke" },
				{ ".ksh","application/x-ksh" },
				{ ".ksh","text/x-script.ksh" },
				{ ".la","audio/nspaudio" },
				{ ".la","audio/x-nspaudio" },
				{ ".lam","audio/x-liveaudio" },
				{ ".latex","application/x-latex" },
				{ ".lha","application/lha" },
				{ ".lha","application/octet-stream" },
				{ ".lha","application/x-lha" },
				{ ".lhx","application/octet-stream" },
				{ ".list","text/plain" },
				{ ".lma","audio/nspaudio" },
				{ ".lma","audio/x-nspaudio" },
				{ ".log","text/plain" },
				{ ".lsp","application/x-lisp" },
				{ ".lsp","text/x-script.lisp" },
				{ ".lst","text/plain" },
				{ ".lsx","text/x-la-asf" },
				{ ".ltx","application/x-latex" },
				{ ".lzh","application/octet-stream" },
				{ ".lzh","application/x-lzh" },
				{ ".lzx","application/lzx" },
				{ ".lzx","application/octet-stream" },
				{ ".lzx","application/x-lzx" },
				{ ".m","text/plain" },
				{ ".m","text/x-m" },
				{ ".m1v","video/mpeg" },
				{ ".m2a","audio/mpeg" },
				{ ".m2v","video/mpeg" },
				{ ".m3u","audio/x-mpequrl" },
				{ ".man","application/x-troff-man" },
				{ ".map","application/x-navimap" },
				{ ".mar","text/plain" },
				{ ".mbd","application/mbedlet" },
				{ ".mc$","application/x-magic-cap-package-1.0" },
				{ ".mcd","application/mcad" },
				{ ".mcd","application/x-mathcad" },
				{ ".mcf","image/vasa" },
				{ ".mcf","text/mcf" },
				{ ".mcp","application/netmc" },
				{ ".me","application/x-troff-me" },
				{ ".mht","message/rfc822" },
				{ ".mhtml","message/rfc822" },
				{ ".mid","application/x-midi" },
				{ ".mid","audio/midi" },
				{ ".mid","audio/x-mid" },
				{ ".mid","audio/x-midi" },
				{ ".mid","music/crescendo" },
				{ ".mid","x-music/x-midi" },
				{ ".midi","application/x-midi" },
				{ ".midi","audio/midi" },
				{ ".midi","audio/x-mid" },
				{ ".midi","audio/x-midi" },
				{ ".midi","music/crescendo" },
				{ ".midi","x-music/x-midi" },
				{ ".mif","application/x-frame" },
				{ ".mif","application/x-mif" },
				{ ".mime","message/rfc822" },
				{ ".mime","www/mime" },
				{ ".mjf","audio/x-vnd.audioexplosion.mjuicemediafile" },
				{ ".mjpg","video/x-motion-jpeg" },
				{ ".mm","application/base64" },
				{ ".mm","application/x-meme" },
				{ ".mme","application/base64" },
				{ ".mod","audio/mod" },
				{ ".mod","audio/x-mod" },
				{ ".moov","video/quicktime" },
				{ ".mov","video/quicktime" },
				{ ".movie","video/x-sgi-movie" },
				{ ".mp2","audio/mpeg" },
				{ ".mp2","audio/x-mpeg" },
				{ ".mp2","video/mpeg" },
				{ ".mp2","video/x-mpeg" },
				{ ".mp2","video/x-mpeq2a" },
				{ ".mp3","audio/mpeg3" },
				{ ".mp3","audio/x-mpeg-3" },
				{ ".mp3","video/mpeg" },
				{ ".mp3","video/x-mpeg" },
				{ ".mpa","audio/mpeg" },
				{ ".mpa","video/mpeg" },
				{ ".mpc","application/x-project" },
				{ ".mpe","video/mpeg" },
				{ ".mpeg","video/mpeg" },
				{ ".mpg","audio/mpeg" },
				{ ".mpg","video/mpeg" },
				{ ".mpga","audio/mpeg" },
				{ ".mpp","application/vnd.ms-project" },
				{ ".mpt","application/x-project" },
				{ ".mpv","application/x-project" },
				{ ".mpx","application/x-project" },
				{ ".mrc","application/marc" },
				{ ".ms","application/x-troff-ms" },
				{ ".mv","video/x-sgi-movie" },
				{ ".my","audio/make" },
				{ ".mzz","application/x-vnd.audioexplosion.mzz" },
				{ ".nap","image/naplps" },
				{ ".naplps","image/naplps" },
				{ ".nc","application/x-netcdf" },
				{ ".ncm","application/vnd.nokia.configuration-message" },
				{ ".nif","image/x-niff" },
				{ ".niff","image/x-niff" },
				{ ".nix","application/x-mix-transfer" },
				{ ".nsc","application/x-conference" },
				{ ".nvd","application/x-navidoc" },
				{ ".o","application/octet-stream" },
				{ ".oda","application/oda" },
				{ ".omc","application/x-omc" },
				{ ".omcd","application/x-omcdatamaker" },
				{ ".omcr","application/x-omcregerator" },
				{ ".p","text/x-pascal" },
				{ ".p10","application/pkcs10" },
				{ ".p10","application/x-pkcs10" },
				{ ".p12","application/pkcs-12" },
				{ ".p12","application/x-pkcs12" },
				{ ".p7a","application/x-pkcs7-signature" },
				{ ".p7c","application/pkcs7-mime" },
				{ ".p7c","application/x-pkcs7-mime" },
				{ ".p7m","application/pkcs7-mime" },
				{ ".p7m","application/x-pkcs7-mime" },
				{ ".p7r","application/x-pkcs7-certreqresp" },
				{ ".p7s","application/pkcs7-signature" },
				{ ".part","application/pro_eng" },
				{ ".pas","text/pascal" },
				{ ".pbm","image/x-portable-bitmap" },
				{ ".pcl","application/vnd.hp-pcl" },
				{ ".pcl","application/x-pcl" },
				{ ".pct","image/x-pict" },
				{ ".pcx","image/x-pcx" },
				{ ".pdb","chemical/x-pdb" },
				{ ".pdf","application/pdf" },
				{ ".pfunk","audio/make" },
				{ ".pfunk","audio/make.my.funk" },
				{ ".pgm","image/x-portable-graymap" },
				{ ".pgm","image/x-portable-greymap" },
				{ ".pic","image/pict" },
				{ ".pict","image/pict" },
				{ ".pkg","application/x-newton-compatible-pkg" },
				{ ".pko","application/vnd.ms-pki.pko" },
				{ ".pl","text/plain" },
				{ ".pl","text/x-script.perl" },
				{ ".plx","application/x-pixclscript" },
				{ ".pm","image/x-xpixmap" },
				{ ".pm","text/x-script.perl-module" },
				{ ".pm4","application/x-pagemaker" },
				{ ".pm5","application/x-pagemaker" },
				{ ".png","image/png" },
				{ ".pnm","application/x-portable-anymap" },
				{ ".pnm","image/x-portable-anymap" },
				{ ".pot","application/mspowerpoint" },
				{ ".pot","application/vnd.ms-powerpoint" },
				{ ".pov","model/x-pov" },
				{ ".ppa","application/vnd.ms-powerpoint" },
				{ ".ppm","image/x-portable-pixmap" },
				{ ".pps","application/mspowerpoint" },
				{ ".pps","application/vnd.ms-powerpoint" },
				{ ".ppt","application/mspowerpoint" },
				{ ".ppt","application/powerpoint" },
				{ ".ppt","application/vnd.ms-powerpoint" },
				{ ".ppt","application/x-mspowerpoint" },
				{ ".ppz","application/mspowerpoint" },
				{ ".pre","application/x-freelance" },
				{ ".prt","application/pro_eng" },
				{ ".ps","application/postscript" },
				{ ".psd","application/octet-stream" },
				{ ".pvu","paleovu/x-pv" },
				{ ".pwz","application/vnd.ms-powerpoint" },
				{ ".py","text/x-script.phyton" },
				{ ".pyc","application/x-bytecode.python" },
				{ ".qcp","audio/vnd.qcelp" },
				{ ".qd3","x-world/x-3dmf" },
				{ ".qd3d","x-world/x-3dmf" },
				{ ".qif","image/x-quicktime" },
				{ ".qt","video/quicktime" },
				{ ".qtc","video/x-qtc" },
				{ ".qti","image/x-quicktime" },
				{ ".qtif","image/x-quicktime" },
				{ ".ra","audio/x-pn-realaudio" },
				{ ".ra","audio/x-pn-realaudio-plugin" },
				{ ".ra","audio/x-realaudio" },
				{ ".ram","audio/x-pn-realaudio" },
				{ ".ras","application/x-cmu-raster" },
				{ ".ras","image/cmu-raster" },
				{ ".ras","image/x-cmu-raster" },
				{ ".rast","image/cmu-raster" },
				{ ".rexx","text/x-script.rexx" },
				{ ".rf","image/vnd.rn-realflash" },
				{ ".rgb","image/x-rgb" },
				{ ".rm","application/vnd.rn-realmedia" },
				{ ".rm","audio/x-pn-realaudio" },
				{ ".rmi","audio/mid" },
				{ ".rmm","audio/x-pn-realaudio" },
				{ ".rmp","audio/x-pn-realaudio" },
				{ ".rmp","audio/x-pn-realaudio-plugin" },
				{ ".rng","application/ringing-tones" },
				{ ".rng","application/vnd.nokia.ringing-tone" },
				{ ".rnx","application/vnd.rn-realplayer" },
				{ ".roff","application/x-troff" },
				{ ".rp","image/vnd.rn-realpix" },
				{ ".rpm","audio/x-pn-realaudio-plugin" },
				{ ".rt","text/richtext" },
				{ ".rt","text/vnd.rn-realtext" },
				{ ".rtf","application/rtf" },
				{ ".rtf","application/x-rtf" },
				{ ".rtf","text/richtext" },
				{ ".rtx","application/rtf" },
				{ ".rtx","text/richtext" },
				{ ".rv","video/vnd.rn-realvideo" },
				{ ".s","text/x-asm" },
				{ ".s3m","audio/s3m" },
				{ ".saveme","application/octet-stream" },
				{ ".sbk","application/x-tbook" },
				{ ".scm","application/x-lotusscreencam" },
				{ ".scm","text/x-script.guile" },
				{ ".scm","text/x-script.scheme" },
				{ ".scm","video/x-scm" },
				{ ".sdml","text/plain" },
				{ ".sdp","application/sdp" },
				{ ".sdp","application/x-sdp" },
				{ ".sdr","application/sounder" },
				{ ".sea","application/sea" },
				{ ".sea","application/x-sea" },
				{ ".set","application/set" },
				{ ".sgm","text/sgml" },
				{ ".sgm","text/x-sgml" },
				{ ".sgml","text/sgml" },
				{ ".sgml","text/x-sgml" },
				{ ".sh","application/x-bsh" },
				{ ".sh","application/x-sh" },
				{ ".sh","application/x-shar" },
				{ ".sh","text/x-script.sh" },
				{ ".shar","application/x-bsh" },
				{ ".shar","application/x-shar" },
				{ ".shtml","text/html" },
				{ ".shtml","text/x-server-parsed-html" },
				{ ".sid","audio/x-psid" },
				{ ".sit","application/x-sit" },
				{ ".sit","application/x-stuffit" },
				{ ".skd","application/x-koan" },
				{ ".skm","application/x-koan" },
				{ ".skp","application/x-koan" },
				{ ".skt","application/x-koan" },
				{ ".sl","application/x-seelogo" },
				{ ".smi","application/smil" },
				{ ".smil","application/smil" },
				{ ".snd","audio/basic" },
				{ ".snd","audio/x-adpcm" },
				{ ".sol","application/solids" },
				{ ".spc","application/x-pkcs7-certificates" },
				{ ".spc","text/x-speech" },
				{ ".spl","application/futuresplash" },
				{ ".spr","application/x-sprite" },
				{ ".sprite","application/x-sprite" },
				{ ".src","application/x-wais-source" },
				{ ".ssi","text/x-server-parsed-html" },
				{ ".ssm","application/streamingmedia" },
				{ ".sst","application/vnd.ms-pki.certstore" },
				{ ".step","application/step" },
				{ ".stl","application/sla" },
				{ ".stl","application/vnd.ms-pki.stl" },
				{ ".stl","application/x-navistyle" },
				{ ".stp","application/step" },
				{ ".sv4cpio","application/x-sv4cpio" },
				{ ".sv4crc","application/x-sv4crc" },
				{ ".svf","image/vnd.dwg" },
				{ ".svf","image/x-dwg" },
				{ ".svr","application/x-world" },
				{ ".svr","x-world/x-svr" },
				{ ".swf","application/x-shockwave-flash" },
				{ ".t","application/x-troff" },
				{ ".talk","text/x-speech" },
				{ ".tar","application/x-tar" },
				{ ".tbk","application/toolbook" },
				{ ".tbk","application/x-tbook" },
				{ ".tcl","application/x-tcl" },
				{ ".tcl","text/x-script.tcl" },
				{ ".tcsh","text/x-script.tcsh" },
				{ ".tex","application/x-tex" },
				{ ".texi","application/x-texinfo" },
				{ ".texinfo","application/x-texinfo" },
				{ ".text","application/plain" },
				{ ".text","text/plain" },
				{ ".tgz","application/gnutar" },
				{ ".tgz","application/x-compressed" },
				{ ".tif","image/tiff" },
				{ ".tif","image/x-tiff" },
				{ ".tiff","image/tiff" },
				{ ".tiff","image/x-tiff" },
				{ ".tr","application/x-troff" },
				{ ".tsi","audio/tsp-audio" },
				{ ".tsp","application/dsptype" },
				{ ".tsp","audio/tsplayer" },
				{ ".tsv","text/tab-separated-values" },
				{ ".turbot","image/florian" },
				{ ".txt","text/plain" },
				{ ".uil","text/x-uil" },
				{ ".uni","text/uri-list" },
				{ ".unis","text/uri-list" },
				{ ".unv","application/i-deas" },
				{ ".uri","text/uri-list" },
				{ ".uris","text/uri-list" },
				{ ".ustar","application/x-ustar" },
				{ ".ustar","multipart/x-ustar" },
				{ ".uu","application/octet-stream" },
				{ ".uu","text/x-uuencode" },
				{ ".uue","text/x-uuencode" },
				{ ".vcd","application/x-cdlink" },
				{ ".vcs","text/x-vcalendar" },
				{ ".vda","application/vda" },
				{ ".vdo","video/vdo" },
				{ ".vew","application/groupwise" },
				{ ".viv","video/vivo" },
				{ ".viv","video/vnd.vivo" },
				{ ".vivo","video/vivo" },
				{ ".vivo","video/vnd.vivo" },
				{ ".vmd","application/vocaltec-media-desc" },
				{ ".vmf","application/vocaltec-media-file" },
				{ ".voc","audio/voc" },
				{ ".voc","audio/x-voc" },
				{ ".vos","video/vosaic" },
				{ ".vox","audio/voxware" },
				{ ".vqe","audio/x-twinvq-plugin" },
				{ ".vqf","audio/x-twinvq" },
				{ ".vql","audio/x-twinvq-plugin" },
				{ ".vrml","application/x-vrml" },
				{ ".vrml","model/vrml" },
				{ ".vrml","x-world/x-vrml" },
				{ ".vrt","x-world/x-vrt" },
				{ ".vsd","application/x-visio" },
				{ ".vst","application/x-visio" },
				{ ".vsw","application/x-visio" },
				{ ".w60","application/wordperfect6.0" },
				{ ".w61","application/wordperfect6.1" },
				{ ".w6w","application/msword" },
				{ ".wav","audio/wav" },
				{ ".wav","audio/x-wav" },
				{ ".wb1","application/x-qpro" },
				{ ".wbmp","image/vnd.wap.wbmp" },
				{ ".web","application/vnd.xara" },
				{ ".wiz","application/msword" },
				{ ".wk1","application/x-123" },
				{ ".wmf","windows/metafile" },
				{ ".wml","text/vnd.wap.wml" },
				{ ".wmlc","application/vnd.wap.wmlc" },
				{ ".wmls","text/vnd.wap.wmlscript" },
				{ ".wmlsc","application/vnd.wap.wmlscriptc" },
				{ ".word","application/msword" },
				{ ".wp","application/wordperfect" },
				{ ".wp5","application/wordperfect" },
				{ ".wp5","application/wordperfect6.0" },
				{ ".wp6","application/wordperfect" },
				{ ".wpd","application/wordperfect" },
				{ ".wpd","application/x-wpwin" },
				{ ".wq1","application/x-lotus" },
				{ ".wri","application/mswrite" },
				{ ".wri","application/x-wri" },
				{ ".wrl","application/x-world" },
				{ ".wrl","model/vrml" },
				{ ".wrl","x-world/x-vrml" },
				{ ".wrz","model/vrml" },
				{ ".wrz","x-world/x-vrml" },
				{ ".wsc","text/scriplet" },
				{ ".wsrc","application/x-wais-source" },
				{ ".wtk","application/x-wintalk" },
				{ ".xbm","image/x-xbitmap" },
				{ ".xbm","image/x-xbm" },
				{ ".xbm","image/xbm" },
				{ ".xdr","video/x-amt-demorun" },
				{ ".xgz","xgl/drawing" },
				{ ".xif","image/vnd.xiff" },
				{ ".xl","application/excel" },
				{ ".xla","application/excel" },
				{ ".xla","application/x-excel" },
				{ ".xla","application/x-msexcel" },
				{ ".xlb","application/excel" },
				{ ".xlb","application/vnd.ms-excel" },
				{ ".xlb","application/x-excel" },
				{ ".xlc","application/excel" },
				{ ".xlc","application/vnd.ms-excel" },
				{ ".xlc","application/x-excel" },
				{ ".xld","application/excel" },
				{ ".xld","application/x-excel" },
				{ ".xlk","application/excel" },
				{ ".xlk","application/x-excel" },
				{ ".xll","application/excel" },
				{ ".xll","application/vnd.ms-excel" },
				{ ".xll","application/x-excel" },
				{ ".xlm","application/excel" },
				{ ".xlm","application/vnd.ms-excel" },
				{ ".xlm","application/x-excel" },
				{ ".xls","application/excel" },
				{ ".xls","application/vnd.ms-excel" },
				{ ".xls","application/x-excel" },
				{ ".xls","application/x-msexcel" },
				{ ".xlt","application/excel" },
				{ ".xlt","application/x-excel" },
				{ ".xlv","application/excel" },
				{ ".xlv","application/x-excel" },
				{ ".xlw","application/excel" },
				{ ".xlw","application/vnd.ms-excel" },
				{ ".xlw","application/x-excel" },
				{ ".xlw","application/x-msexcel" },
				{ ".xm","audio/xm" },
				{ ".xml","application/xml" },
				{ ".xml","text/xml" },
				{ ".xmz","xgl/movie" },
				{ ".xpix","application/x-vnd.ls-xpix" },
				{ ".xpm","image/x-xpixmap" },
				{ ".xpm","image/xpm" },
				{ ".x-png","image/png" },
				{ ".xsr","video/x-amt-showrun" },
				{ ".xwd","image/x-xwd" },
				{ ".xwd","image/x-xwindowdump" },
				{ ".xyz","chemical/x-pdb" },
				{ ".z","application/x-compress" },
				{ ".z","application/x-compressed" },
				{ ".zip","application/x-compressed" },
				{ ".zip","application/x-zip-compressed" },
				{ ".zip","application/zip" },
				{ ".zip","multipart/x-zip" },
				{ ".zoo","application/octet-stream" },
				{ ".zsh","text/x-script.zsh" }
			};

			std::unordered_multimap<std::string, std::string, CaseInsensitiveHash, CaseInsensitiveEqual> headers;

            size_t size() {
                return streambuf.size();
            }

            /// If true, force server to close the connection after the response have been sent.
            ///
            /// This is useful when implementing a HTTP/1.0-server sending content
            /// without specifying the content length.
            bool close_connection_after_response = false;

			void response(int status)
			{
				if (http_status_codes.find(status) == http_status_codes.end())
					throw std::runtime_error("error status code: " + std::to_string(status));
				this->status = status;
			}

			void error(int status, const std::string & content = std::string(),bool noBody = false)
			{
				auto it = http_status_codes.find(status);
				if (it == http_status_codes.end())
				{
					status = 500;
					close_connection_after_response = true;
				}

				auto reason = http_status_codes[status];

				if(noBody)
				{
					*this << "HTTP/1.1 " << status << " " << reason << "\r\n";
				}
				else if (content == "")
				{
					*this << "HTTP/1.1 " << status << " " << reason << "\r\nContent-Length: " << reason.length() << "\r\n" << reason;
				}
				else
				{
					*this << "HTTP/1.1 " << status << " " << reason << "\r\nContent-Length: " << content.length() << "\r\n" << content;
				}

				for (auto header : headers)
				{
					*this << header.first << ": " << header.second << "\r\n";
				}
				*this << "\r\n";
			}

			void set_MIME(const std::string & ext)
			{
				auto it = http_mime_types.find(ext);
				if(it != http_mime_types.end())
				{
					headers.emplace("Content-Type", it->second);
				}
			}

			void set_cookie(const std::string & name,const std::string & value, std::time_t Expires = 0,int Max_Age = 0,std::string domain = "",std::string path = "",bool secure = false,bool HttpOnly = false)
			{
				std::stringstream cookie;
				cookie << name << "=" << value;
				
				if(Expires)
				{
					cookie << "; Expires=" << std::put_time(std::gmtime(&Expires), "%a, %d %b %Y %H:%M:%S GMT");
				}

				if(Max_Age)
				{
					cookie << "; Max-Age=" << Max_Age;
				}

				if(domain!="")
				{
					cookie << "; Domain=" << domain;
				}

				if(path!="")
				{
					cookie << "; Path=" << path;
				}

				if(secure)
				{
					cookie << "; Secure";
				}

				if(HttpOnly)
				{
					cookie << "; HttpOnly";
				}

				headers.emplace("Set-Cookie", cookie.str());
			}

			void send_headers()
			{
				*this << "HTTP/1.1 " << status << " " << http_status_codes[status] << "\r\n";
				for (auto header : headers)
				{
					*this << header.first << ": " << header.second << "\r\n";
				}
				*this << "\r\n";
			}
        };
        
        class Content : public std::istream {
            friend class ServerBase<socket_type>;
        public:
            size_t size() {
                return streambuf.size();
            }
            std::string string() {
                std::stringstream ss;
                ss << rdbuf();
                return ss.str();
            }
			
        private:
            asio::streambuf &streambuf;
            Content(asio::streambuf &streambuf): std::istream(&streambuf), streambuf(streambuf) {}
        };
        
        class Request {
            friend class ServerBase<socket_type>;
            friend class Server<socket_type>;
        public:
            std::string method, path, http_version;

            Content content;

            std::unordered_multimap<std::string, std::string, CaseInsensitiveHash, CaseInsensitiveEqual> header;

            regex::smatch path_match;
            
            std::string remote_endpoint_address;
            unsigned short remote_endpoint_port;
            
            /// Returns query keys with percent-decoded values.
            std::unordered_multimap<std::string, std::string, CaseInsensitiveHash, CaseInsensitiveEqual> parse_query_string() {
                std::unordered_multimap<std::string, std::string, CaseInsensitiveHash, CaseInsensitiveEqual> result;
                auto qs_start_pos = path.find('?');
                if (qs_start_pos != std::string::npos && qs_start_pos + 1 < path.size()) {
                    ++qs_start_pos;
                    static regex::regex pattern("([\\w+%]+)=?([^&]*)");
                    int submatches[] = {1, 2};
                    auto it_begin = regex::sregex_token_iterator(path.begin() + qs_start_pos, path.end(), pattern, submatches);
                    auto it_end = regex::sregex_token_iterator();
                    for (auto it = it_begin; it != it_end; ++it) {
                        auto submatch1=it->str();
                        auto submatch2=(++it)->str();
                        auto query_it = result.emplace(submatch1, submatch2);
                        auto &value = query_it->second;
                        for (size_t c = 0; c < value.size(); ++c) {
                            if (value[c] == '+')
                                value[c] = ' ';
                            else if (value[c] == '%' && c + 2 < value.size()) {
                                auto hex = value.substr(c + 1, 2);
                                auto chr = static_cast<char>(std::strtol(hex.c_str(), nullptr, 16));
                                value.replace(c, 3, &chr, 1);
                            }
                        }
                    }
                }
                return result;
            }

			std::unordered_multimap<std::string, std::string, CaseInsensitiveHash, CaseInsensitiveEqual> parse_cookies()
            {
				std::unordered_multimap<std::string, std::string, CaseInsensitiveHash, CaseInsensitiveEqual> result;
				auto it = this->header.find("Cookie");
				if(it==this->header.end())	return result;

				auto cookie = it->second;

				static regex::regex pattern("([\\w+%]+)=?([^;]*)");
				int submatches[] = { 1, 2 };
				auto it_begin = regex::sregex_token_iterator(cookie.begin(), cookie.end(), pattern, submatches);
				auto it_end = regex::sregex_token_iterator();
				for (auto it = it_begin; it != it_end; ++it) {
					auto submatch1 = it->str();
					auto submatch2 = (++it)->str();
					result.emplace(submatch1, submatch2);
				}

				return result;
            }

			std::unordered_multimap<std::string, std::string, CaseInsensitiveHash, CaseInsensitiveEqual> parse_post()
            {
				std::unordered_multimap<std::string, std::string, CaseInsensitiveHash, CaseInsensitiveEqual> result;

				auto it = this->header.find("Content-Type");
				if (it == this->header.end())	return result;

				std::string type = it->second;
				if(type == "application/x-www-form-urlencoded")
				{
					std::string cont = content.string();
					static regex::regex pattern("([\\w+%]+)=?([^&]*)");
					int submatches[] = { 1, 2 };
					auto it_begin = regex::sregex_token_iterator(cont.begin(), cont.end(), pattern, submatches);
					auto it_end = regex::sregex_token_iterator();
					for (auto it = it_begin; it != it_end; ++it) {
						auto submatch1 = it->str();
						auto submatch2 = url_decode((++it)->str());
						result.emplace(submatch1, submatch2);
					}
				}
				else if(type == "application/json")
				{
					throw std::runtime_error("could not parse json data");
				}
				else if(type == "application/xml")
				{
					throw std::runtime_error("could not parse xml data");
				}
				else if(type.find("multipart/form-data")!=std::string::npos)
				{
					auto st = type.find("boundary=");
					auto boundary = type.substr(st+9);
					std::vector<std::string> conts(std::istream_iterator<std::string>(content), {});
					for(auto it =conts.begin();it < conts.end() ; ++it)
					{
						while(it->find("name") == std::string::npos)
						{
							++it;
							if (it == conts.end())
								return result;
						}
						auto name = it->substr(6,it->length() - 7);
						++it;
						result.emplace(name, *it);
					}
				}
				else
				{
					throw std::runtime_error("unknown type");
				}

				return result;
            }

        private:
            Request(const socket_type &socket): content(streambuf) {
                try {
                    remote_endpoint_address=socket.lowest_layer().remote_endpoint().address().to_string();
                    remote_endpoint_port=socket.lowest_layer().remote_endpoint().port();
                }
                catch(...) {}
            }
            
            asio::streambuf streambuf;
        };
        
        class Config {
            friend class ServerBase<socket_type>;

            Config(unsigned short port): port(port) {}
        public:
            /// Port number to use. Defaults to 80 for HTTP and 443 for HTTPS.
            unsigned short port;
            /// Number of threads that the server will use when start() is called. Defaults to 1 thread.
            size_t thread_pool_size=1;
            /// Timeout on request handling. Defaults to 5 seconds.
            size_t timeout_request=5;
            /// Timeout on content handling. Defaults to 300 seconds.
            size_t timeout_content=300;
            /// IPv4 address in dotted decimal form or IPv6 address in hexadecimal notation.
            /// If empty, the address will be any address.
            std::string address;
            /// Set to false to avoid binding the socket to an address that is already in use. Defaults to true.
            bool reuse_address=true;
        };
        ///Set before calling start().
        Config config;
        
    private:
        class regex_orderable : public regex::regex {
            std::string str;
        public:
            regex_orderable(const char *regex_cstr) : regex::regex(regex_cstr), str(regex_cstr) {}
            regex_orderable(const std::string &regex_str) : regex::regex(regex_str), str(regex_str) {}
            bool operator<(const regex_orderable &rhs) const {
                return str<rhs.str;
            }
        };
    public:
        /// Warning: do not add or remove resources after start() is called
        std::map<regex_orderable, std::map<std::string,
            std::function<void(std::shared_ptr<typename ServerBase<socket_type>::Response>, std::shared_ptr<typename ServerBase<socket_type>::Request>)> > > resource;
        
        std::map<std::string,
            std::function<void(std::shared_ptr<typename ServerBase<socket_type>::Response>, std::shared_ptr<typename ServerBase<socket_type>::Request>)> > default_resource;
        
        std::function<void(std::shared_ptr<typename ServerBase<socket_type>::Request>, const error_code&)> on_error;
        
        std::function<void(std::shared_ptr<socket_type> socket, std::shared_ptr<typename ServerBase<socket_type>::Request>)> on_upgrade;
        
        virtual void start() {
            if(!io_service)
                io_service=std::make_shared<asio::io_service>();

            if(io_service->stopped())
                io_service->reset();

            asio::ip::tcp::endpoint endpoint;
            if(config.address.size()>0)
                endpoint=asio::ip::tcp::endpoint(asio::ip::address::from_string(config.address), config.port);
            else
                endpoint=asio::ip::tcp::endpoint(asio::ip::tcp::v4(), config.port);
            
            if(!acceptor)
                acceptor=std::unique_ptr<asio::ip::tcp::acceptor>(new asio::ip::tcp::acceptor(*io_service));
            acceptor->open(endpoint.protocol());
            acceptor->set_option(asio::socket_base::reuse_address(config.reuse_address));
            acceptor->bind(endpoint);
            acceptor->listen();
     
            accept(); 
            
            //If thread_pool_size>1, start m_io_service.run() in (thread_pool_size-1) threads for thread-pooling
            threads.clear();
            for(size_t c=1;c<config.thread_pool_size;c++) {
                threads.emplace_back([this]() {
                    io_service->run();
                });
            }

            //Main thread
            if(config.thread_pool_size>0)
                io_service->run();

            //Wait for the rest of the threads, if any, to finish as well
            for(auto& t: threads) {
                t.join();
            }
        }
        
        void stop() {
            acceptor->close();
            if(config.thread_pool_size>0)
                io_service->stop();
        }
        
        ///Use this function if you need to recursively send parts of a longer message
        void send(const std::shared_ptr<Response> &response, const std::function<void(const error_code&)>& callback=nullptr) const {
            asio::async_write(*response->socket, response->streambuf, [this, response, callback](const error_code& ec, size_t /*bytes_transferred*/) {
                if(callback)
                    callback(ec);
            });
        }

        /// If you have your own asio::io_service, store its pointer here before running start().
        /// You might also want to set config.thread_pool_size to 0.
        std::shared_ptr<asio::io_service> io_service;
    protected:
        std::unique_ptr<asio::ip::tcp::acceptor> acceptor;
        std::vector<std::thread> threads;
        
        ServerBase(unsigned short port) : config(port){}
        
        virtual void accept()=0;
        
        std::shared_ptr<asio::deadline_timer> get_timeout_timer(const std::shared_ptr<socket_type> &socket, long seconds) {
            if(seconds==0)
                return nullptr;
            
            auto timer=std::make_shared<asio::deadline_timer>(*io_service);
            timer->expires_from_now(boost::posix_time::seconds(seconds));
            timer->async_wait([socket](const error_code& ec){
                if(!ec) {
                    error_code ec;
                    socket->lowest_layer().shutdown(asio::ip::tcp::socket::shutdown_both, ec);
                    socket->lowest_layer().close();
                }
            });
            return timer;
        }
        
        void read_request_and_content(const std::shared_ptr<socket_type> &socket) {
            //Create new streambuf (Request::streambuf) for async_read_until()
            //shared_ptr is used to pass temporary objects to the asynchronous functions
            std::shared_ptr<Request> request(new Request(*socket));

            //Set timeout on the following asio::async-read or write function
            auto timer=this->get_timeout_timer(socket, config.timeout_request);
                        
            asio::async_read_until(*socket, request->streambuf, "\r\n\r\n",
                    [this, socket, request, timer](const error_code& ec, size_t bytes_transferred) {
                if(timer)
                    timer->cancel();
                if(!ec) {
                    //request->streambuf.size() is not necessarily the same as bytes_transferred, from Boost-docs:
                    //"After a successful async_read_until operation, the streambuf may contain additional data beyond the delimiter"
                    //The chosen solution is to extract lines from the stream directly when parsing the header. What is left of the
                    //streambuf (maybe some bytes of the content) is appended to in the async_read-function below (for retrieving content).
                    size_t num_additional_bytes=request->streambuf.size()-bytes_transferred;
                    
                    if(!this->parse_request(request))
                        return;
                    
                    //If content, read that as well
                    auto it=request->header.find("Content-Length");
                    if(it!=request->header.end()) {
                        unsigned long long content_length;
                        try {
                            content_length=stoull(it->second);
                        }
                        catch(const std::exception &e) {
                            if(on_error)
                                on_error(request, make_error_code::make_error_code(errc::protocol_error));
                            return;
                        }
                        if(content_length>num_additional_bytes) {
                            //Set timeout on the following asio::async-read or write function
                            auto timer=this->get_timeout_timer(socket, config.timeout_content);
                            asio::async_read(*socket, request->streambuf,
                                    asio::transfer_exactly(content_length-num_additional_bytes),
                                    [this, socket, request, timer]
                                    (const error_code& ec, size_t /*bytes_transferred*/) {
                                if(timer)
                                    timer->cancel();
                                if(!ec)
                                    this->find_resource(socket, request);
                                else if(on_error)
                                    on_error(request, ec);
                            });
                        }
                        else
                            this->find_resource(socket, request);
                    }
                    else
                        this->find_resource(socket, request);
                }
                else if(on_error)
                    on_error(request, ec);
            });
        }

        bool parse_request(const std::shared_ptr<Request> &request) const {
            std::string line;
            getline(request->content, line);
            size_t method_end;
            if((method_end=line.find(' '))!=std::string::npos) {
                size_t path_end;
                if((path_end=line.find(' ', method_end+1))!=std::string::npos) {
                    request->method=line.substr(0, method_end);
                    request->path=line.substr(method_end+1, path_end-method_end-1);

                    size_t protocol_end;
                    if((protocol_end=line.find('/', path_end+1))!=std::string::npos) {
                        if(line.compare(path_end+1, protocol_end-path_end-1, "HTTP")!=0)
                            return false;
                        request->http_version=line.substr(protocol_end+1, line.size()-protocol_end-2);
                    }
                    else
                        return false;

                    getline(request->content, line);
                    size_t param_end;
                    while((param_end=line.find(':'))!=std::string::npos) {
                        size_t value_start=param_end+1;
                        if((value_start)<line.size()) {
                            if(line[value_start]==' ')
                                value_start++;
                            if(value_start<line.size())
                                request->header.emplace(line.substr(0, param_end), line.substr(value_start, line.size()-value_start-1));
                        }
    
                        getline(request->content, line);
                    }
                }
                else
                    return false;
            }
            else
                return false;
            return true;
        }

        void find_resource(const std::shared_ptr<socket_type> &socket, const std::shared_ptr<Request> &request) {
            //Upgrade connection
            if(on_upgrade) {
                auto it=request->header.find("Upgrade");
                if(it!=request->header.end()) {
                    on_upgrade(socket, request);
                    return;
                }
            }
            //Find path- and method-match, and call write_response
            for(auto &regex_method: resource) {
                auto it=regex_method.second.find(request->method);
                if(it!=regex_method.second.end()) {
                    regex::smatch sm_res;
                    if(regex::regex_match(request->path, sm_res, regex_method.first)) {
                        request->path_match=std::move(sm_res);
                        write_response(socket, request, it->second);
                        return;
                    }
                }
            }
            auto it=default_resource.find(request->method);
            if(it!=default_resource.end()) {
                write_response(socket, request, it->second);
            }
        }
        
        void write_response(const std::shared_ptr<socket_type> &socket, const std::shared_ptr<Request> &request, 
                std::function<void(std::shared_ptr<typename ServerBase<socket_type>::Response>,
                                   std::shared_ptr<typename ServerBase<socket_type>::Request>)>& resource_function) {
            //Set timeout on the following asio::async-read or write function
            auto timer=this->get_timeout_timer(socket, config.timeout_content);

            auto response=std::shared_ptr<Response>(new Response(socket), [this, request, timer](Response *response_ptr) {
                auto response=std::shared_ptr<Response>(response_ptr);
                this->send(response, [this, response, request, timer](const error_code& ec) {
                    if(timer)
                        timer->cancel();
                    if(!ec) {
                        if (response->close_connection_after_response)
                            return;

                        auto range=request->header.equal_range("Connection");
                        for(auto it=range.first;it!=range.second;it++) {
                            if(case_insensitive_equal(it->second, "close")) {
                                return;
                            } else if (case_insensitive_equal(it->second, "keep-alive")) {
                                this->read_request_and_content(response->socket);
                                return;
                            }
                        }
                        if(request->http_version >= "1.1")
                            this->read_request_and_content(response->socket);
                    }
                    else if(on_error)
                        on_error(request, ec);
                });
            });

            try {
                resource_function(response, request);
            }
            catch(const std::exception &e) {
                if(on_error)
                    on_error(request, make_error_code::make_error_code(errc::operation_canceled));
                return;
            }
        }
    };
    
    template<class socket_type>
    class Server : public ServerBase<socket_type> {};
    
    typedef asio::ip::tcp::socket HTTP;
    
    template<>
    class Server<HTTP> : public ServerBase<HTTP> {
    public:
        DEPRECATED Server(unsigned short port, size_t thread_pool_size=1, long timeout_request=5, long timeout_content=300) :
                Server() {
            config.port=port;
            config.thread_pool_size=thread_pool_size;
            config.timeout_request=timeout_request;
            config.timeout_content=timeout_content;
        }
        
        Server() : ServerBase<HTTP>::ServerBase(80) {}
        
    protected:
        void accept() {
            //Create new socket for this connection
            //Shared_ptr is used to pass temporary objects to the asynchronous functions
            auto socket=std::make_shared<HTTP>(*io_service);
                        
            acceptor->async_accept(*socket, [this, socket](const error_code& ec){
                //Immediately start accepting a new connection (if io_service hasn't been stopped)
                if (ec != asio::error::operation_aborted)
                    accept();
                                
                if(!ec) {
                    asio::ip::tcp::no_delay option(true);
                    socket->set_option(option);
                    
                    this->read_request_and_content(socket);
                }
                else if(on_error)
                    on_error(std::shared_ptr<Request>(new Request(*socket)), ec);
            });
        }
    };
}
#endif	/* SERVER_HTTP_HPP */
