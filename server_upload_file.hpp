#ifndef SERVER_UPLOAD_FILE_HPP
#define	SERVER_UPLOAD_FILE_HPP

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/regex.hpp>

#include <unordered_map>
#include <thread>
#include <functional>
#include <iostream>
#include <sstream>
#include "server_http.hpp"

namespace SimpleWeb {
	template<class socket_type>
	class UploadFile {
		friend class ServerBase<socket_type>;
	public:
		std::string boundary;
		std::string content_data;
		size_t content_data_len;

		std::string processing_field_name;
		std::string filename;
		std::string content_type;
		std::string error_message;
	public:
		UploadFile() :content_data_len(0){};
		virtual ~UploadFile(){};

		void reset(){
			boundary = "";
			content_data = "";
			content_data_len = 0;
			processing_field_name = "";
			filename = "";
			content_type = "";
			error_message = "";
		}

		
		int parse_upload_request(std::shared_ptr<typename ServerBase<socket_type>::Request> request)
		{
			reset();

			auto content = request->content.string();

			//process Content-Type
			const auto content_type_iter = request->header.find("Content-Type");
			if (content_type_iter == request->header.end())
			{
				error_message = std::string("Cannot find Content-Type in Header") + std::string("\"");
				return -1;
			}

			auto content_type = content_type_iter->second;

			size_t boundary_pos = content_type.find("boundary=");

			if (boundary_pos == std::string::npos) {
				error_message = std::string("Cannot find boundary in Content-type: \"") + content_type + std::string("\"");
				return -2;
			}

			boundary = content_type.substr(boundary_pos + 9, content_type.length() - boundary_pos);

			//parse Headers
			size_t content_data_begin_pos = content.find("\r\n\r\n");
			std::string content_header = content.substr(0, content_data_begin_pos);

			if (content_data_begin_pos == string::npos)
			{
				error_message = std::string("Cannot find content data");
				return -3;
			}

			content_data_begin_pos += 4;//skip "\r\n\r\n"

			size_t content_data_end_pos = content.find(boundary, content_data_begin_pos);
			if (content_data_end_pos == string::npos)
			{
				content_data_end_pos = content.length() - boundary.length() - 6;//exclude "\r\n--boundary--"
			}
			else
			{
				content_data_end_pos -= 4;//exclude "\r\n--
			}

			content_data_len = content_data_end_pos - content_data_begin_pos;
			content_data = content.substr(content_data_begin_pos, content_data_len);

			//check  Content-Disposition is "form-data"
			if (content_header.find("Content-Disposition: form-data;") == std::string::npos)
			{
				error_message = std::string("Accepted headers of field does not contain \"Content-Disposition: form-data;\"\nThe headers are: \"") + content_header + std::string("\"");
				return -4;
			}

			// Find name
			size_t name_pos = content_header.find("name=\"");
			if (name_pos == std::string::npos)
			{
				error_message = std::string("Accepted headers of field does not contain \"name=\".\nThe headers are: \"") + content_header + std::string("\"");
				return -5;
			}

			name_pos += 6;//SKIP "name=\""
			size_t name_end_pos = content_header.find("\"", name_pos);
			if (name_end_pos == std::string::npos)
			{
				error_message = std::string("Cannot find closing quote of \"name=\" attribute.\nThe headers are: \"") + content_header + std::string("\"");
				return -6;
			}
			else
			{
				processing_field_name = content_header.substr(name_pos, name_end_pos - name_pos);
			}


			// find filename if exists
			size_t filename_pos = content_header.find("filename=\"");
			if (filename_pos == std::string::npos)
			{
				error_message = std::string("Accepted headers of field does not contain \"filename=\".\nThe headers are: \"") + content_header + std::string("\"");
				return -7;
			}

			filename_pos += 10;//SKIP "filename=\""
			size_t filename_end_pos = content_header.find("\"", filename_pos);
			if (filename_end_pos == std::string::npos)
			{
				error_message = std::string("Cannot find closing quote of \"filename=\" attribute.\nThe headers are: \"") + content_header + std::string("\"");
				return -8;
			}
			else
			{
				filename = content_header.substr(filename_pos, filename_end_pos - filename_pos);
			}

			// find Content-Type if exists
			size_t content_type_pos = content_header.find("Content-Type: ");
			if (content_type_pos == std::string::npos)
			{
				error_message = std::string("Cannot find closing quote of \"filename=\" attribute.\nThe headers are: \"") + content_header + std::string("\"");
				return -9;
			}

			content_type_pos += 14;//SKIP "Content-Type: "
			size_t content_type_end_pos = content_header.find("\r\n", content_type_pos);
			if (content_type_end_pos != std::string::npos)
			{
				content_type = content_header.substr(content_type_pos, content_type_end_pos - content_type_pos);
			}
			else
			{
				content_type = content_header.substr(content_type_pos);
			}

			return 0;
		};
		
		bool save_content_to_file(std::string save_to_filepath)
		{
			if (content_data.length() > 0 && content_data.length() == content_data_len)
			{
				FILE* fsSave = NULL;
				fopen_s(&fsSave, save_to_filepath.c_str(), "wb");
				if (fsSave == NULL)
				{
					return false;
				}
				fwrite(content_data.c_str(), 1, content_data_len, fsSave);
				fclose(fsSave);
				return true;
			}
			return false;
		}
	};
};
#endif	/* SERVER_UPLOAD_FILE_HPP */
