/* 
  FSWebServer - Example WebServer with SPIFFS backend for esp8266
  Copyright (c) 2015 Hristo Gochkov. All rights reserved.
  This file is part of the ESP8266WebServer library for Arduino environment.
 
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
  
  upload the contents of the data folder with MkSPIFFS Tool ("ESP8266 Sketch Data Upload" in Tools menu in Arduino IDE)
  or you can upload the contents of a folder if you CD in that folder and run the following command:
  for file in `ls -A1`; do curl -F "file=@$PWD/$file" esp8266fs.local/edit; done
  
  access the sample web page at http://esp8266fs.local
  edit the page by going to http://esp8266fs.local/edit
*/


#include "FSBrowser.h"

FSBrowser::FSBrowser(ESP8266WebServer & HTTP, fs::FS & fs): _HTTP(HTTP), _FS(fs){}

FSBrowser::~FSBrowser(){};


void FSBrowser::begin() {

Serial.println("SPIFFS FILES:");
    {
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {    
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      DBG_OUTPUT_PORT.printf("     FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());
    }
    DBG_OUTPUT_PORT.printf("\n");
  }


	  //SERVER INIT
  //list directory
  _HTTP.on("/list", HTTP_GET, std::bind(&FSBrowser::handleFileList,this) );
  //load editor
  _HTTP.on("/edit", HTTP_GET, [this](){
    if(!handleFileRead("/edit.htm")) _HTTP.send(404, "text/plain", "FileNotFound");
  });
  //create file
  _HTTP.on("/edit", HTTP_PUT, std::bind(&FSBrowser::handleFileCreate,this) );
  //delete file
  _HTTP.on("/edit", HTTP_DELETE, std::bind(&FSBrowser::handleFileDelete  , this) );
  //first callback is called after the request has ended with all parsed arguments
  //second callback handles file uploads at that location
  _HTTP.on("/edit", HTTP_POST, [this](){ _HTTP.send(200, "text/plain", ""); }, std::bind(&FSBrowser::handleFileUpload  , this));

  //called when the url is not defined here
  //use it to load content from SPIFFS
  _HTTP.onNotFound([this](){
    if(!handleFileRead(_HTTP.uri()))
      _HTTP.send(404, "text/plain", "FileNotFound");
  });
}


//format bytes
String FSBrowser::formatBytes(size_t bytes){
  if (bytes < 1024){
    return String(bytes)+"B";
  } else if(bytes < (1024 * 1024)){
    return String(bytes/1024.0)+"KB";
  } else if(bytes < (1024 * 1024 * 1024)){
    return String(bytes/1024.0/1024.0)+"MB";
  } else {
    return String(bytes/1024.0/1024.0/1024.0)+"GB";
  }
}

String FSBrowser::getContentType(String filename){
  if(_HTTP.hasArg("download")) return "application/octet-stream";
  else if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}


bool FSBrowser::handleFileRead(String path){
  DBG_OUTPUT_PORT.println("handleFileRead: " + path);
  if(path.endsWith("/")) path += "index.htm";
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if(_FS.exists(pathWithGz) || _FS.exists(path)){
    if(_FS.exists(pathWithGz))
      path += ".gz";
    File file = _FS.open(path, "r");
    size_t sent = _HTTP.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void FSBrowser::handleFileUpload(){
  if(_HTTP.uri() != "/edit") return;
  HTTPUpload& upload = _HTTP.upload();
  if(upload.status == UPLOAD_FILE_START){
    String filename = upload.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    DBG_OUTPUT_PORT.print("handleFileUpload Name: "); DBG_OUTPUT_PORT.println(filename);
    fsUploadFile = _FS.open(filename, "w");
    filename = String();
  } else if(upload.status == UPLOAD_FILE_WRITE){
    //DBG_OUTPUT_PORT.print("handleFileUpload Data: "); DBG_OUTPUT_PORT.println(upload.currentSize);
    if(fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
  } else if(upload.status == UPLOAD_FILE_END){
    if(fsUploadFile)
      fsUploadFile.close();
    DBG_OUTPUT_PORT.print("handleFileUpload Size: "); DBG_OUTPUT_PORT.println(upload.totalSize);
  }
}

void FSBrowser::handleFileDelete(){
  if(_HTTP.args() == 0) return _HTTP.send(500, "text/plain", "BAD ARGS");
  String path = _HTTP.arg(0);
  DBG_OUTPUT_PORT.println("handleFileDelete: " + path);
  if(path == "/")
    return _HTTP.send(500, "text/plain", "BAD PATH");
  if(!_FS.exists(path))
    return _HTTP.send(404, "text/plain", "FileNotFound");
  _FS.remove(path);
  _HTTP.send(200, "text/plain", "");
  path = String();
}

void FSBrowser::handleFileCreate(){
  if(_HTTP.args() == 0)
    return _HTTP.send(500, "text/plain", "BAD ARGS");
  String path = _HTTP.arg(0);
  DBG_OUTPUT_PORT.println("handleFileCreate: " + path);
  if(path == "/")
    return _HTTP.send(500, "text/plain", "BAD PATH");
  if(_FS.exists(path))
    return _HTTP.send(500, "text/plain", "FILE EXISTS");
  File file = _FS.open(path, "w");
  if(file)
    file.close();
  else
    return _HTTP.send(500, "text/plain", "CREATE FAILED");
  _HTTP.send(200, "text/plain", "");
  path = String();
}

void FSBrowser::handleFileList() {
  if(!_HTTP.hasArg("dir")) {_HTTP.send(500, "text/plain", "BAD ARGS"); return;}
  
  String path = _HTTP.arg("dir");
  DBG_OUTPUT_PORT.println("handleFileList: " + path);
  Dir dir = _FS.openDir(path);
  path = String();

  String output = "[";
  while(dir.next()){
    File entry = dir.openFile("r");
    if (output != "[") output += ',';
    bool isDir = false;
    output += "{\"type\":\"";
    output += (isDir)?"dir":"file";
    output += "\",\"name\":\"";
    output += String(entry.name()).substring(1);
    output += "\"}";
    entry.close();
  }
  
  output += "]";
  _HTTP.send(200, "text/json", output);
}


