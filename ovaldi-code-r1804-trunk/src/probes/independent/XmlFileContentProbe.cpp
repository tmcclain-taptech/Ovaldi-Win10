//
//
//****************************************************************************************//
// Copyright (c) 2002-2014, The MITRE Corporation
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright notice, this list
//       of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright notice, this 
//       list of conditions and the following disclaimer in the documentation and/or other
//       materials provided with the distribution.
//     * Neither the name of The MITRE Corporation nor the names of its contributors may be
//       used to endorse or promote products derived from this software without specific 
//       prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY 
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
// OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
// SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
// OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
// TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
// EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//****************************************************************************************//

// Xalan and Xerces includes
#include <xercesc/framework/LocalFileInputSource.hpp>
#include <xercesc/sax/EntityResolver.hpp>
#include <xercesc/sax/SAXException.hpp>
#include <xercesc/sax/SAXParseException.hpp>
#include <xercesc/sax/InputSource.hpp>
#include <xercesc/util/BinInputStream.hpp>
#include <xalanc/PlatformSupport/XSLException.hpp>
#include <xalanc/DOMSupport/XalanDocumentPrefixResolver.hpp>
#include <xalanc/XPath/XObject.hpp>
#include <xalanc/XPath/NodeRefList.hpp>
#include <xalanc/XPath/XPathEvaluator.hpp>
#include <xalanc/XalanSourceTree/XalanSourceTreeDOMSupport.hpp>
#include <xalanc/XalanSourceTree/XalanSourceTreeInit.hpp>
#include <xalanc/XalanSourceTree/XalanSourceTreeParserLiaison.hpp>

#ifdef WIN32
#  include <FsRedirectionGuard.h>
#  include <PrivilegeGuard.h>
// macro this so it can disappear on non-windows OSs.
#  define ADD_WINDOWS_VIEW_ENTITY \
	item->AppendElement(new ItemEntity("windows_view", \
		(fileFinder.GetView() == BIT_32 ? "32_bit" : "64_bit")));
#else
#  define ADD_WINDOWS_VIEW_ENTITY
#  define FS_REDIRECT_GUARD_BEGIN(x)
#  define FS_REDIRECT_GUARD_END
#endif

#include "XmlCommon.h"
#include "FileFinder.h"

#include "XmlFileContentProbe.h"

using namespace std;
using namespace xercesc;
using namespace xalanc;

namespace {
	/**
	 * This class is an entity resolver that disables DTD resolution when
	 * parsing an XML document.  This prevents unwanted network accesses.
	 */
	class DummyEntityResolver : public xercesc::EntityResolver
	{
	public:
		virtual xercesc::InputSource* resolveEntity(const XMLCh *const publicId, const XMLCh *const systemId);

	private:
		/**
		 * An InputSource implementation which always returns a DoNothingBinInputStream.
		 */
		class NoOpInputSource : public xercesc::InputSource
		{
		public:
			virtual xercesc::BinInputStream* makeStream() const;
		};

		/**
		 * A BinInputStream implementation which does nothing.  Both of its methods
		 * simply return 0.
		 */
		class DoNothingBinInputStream : public xercesc::BinInputStream
		{
		public:
			virtual XMLFilePos curPos() const;
			virtual XMLSize_t readBytes(XMLByte *const toFill, const XMLSize_t maxToRead);
			virtual const XMLCh *getContentType() const;
		};
	};
}

XmlFileContentProbe* XmlFileContentProbe::instance = NULL;

XmlFileContentProbe::XmlFileContentProbe() {

}

XmlFileContentProbe::~XmlFileContentProbe() {
  instance = NULL;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  Public Members  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
AbsProbe* XmlFileContentProbe::Instance() {

	// Use lazy initialization
	if(instance == NULL) 
		instance = new XmlFileContentProbe();

	return instance;	
}

ItemVector* XmlFileContentProbe::CollectItems(Object* object) {

	// get the hive, key, and name from the provided object
	ObjectEntity* path = object->GetElementByName("path");
	ObjectEntity* fileName = object->GetElementByName("filename");
    ObjectEntity* filePath = object->GetElementByName("filepath");
	ObjectEntity* xpath = object->GetElementByName("xpath");
	
	ItemVector *collectedItems = new ItemVector();
#ifdef WIN32
	FileFinder fileFinder(WindowsCommon::behavior2view(object->GetBehaviors()));
#else
	FileFinder fileFinder;
#endif
	StringPairVector* filePaths = NULL;

#ifdef WIN32
	{
		PrivilegeGuard pg(SE_BACKUP_NAME, false);
#endif
	
		if(filePath != NULL){
			filePaths = fileFinder.SearchFiles(filePath);	
		}else{
			filePaths = fileFinder.SearchFiles(path, fileName, object->GetBehaviors());
		}

#ifdef WIN32
	}
#endif

	if(filePaths != NULL && filePaths->size() > 0) {
		// Loop through all file paths
		StringPairVector::iterator iterator;
		for(iterator = filePaths->begin(); iterator != filePaths->end(); iterator++) {

			StringPair* fp = (*iterator);

			if(fp->second.compare("") == 0) {

				Item* item = NULL;

				// check if the code should report that the filename does not exist.
				if(fileFinder.ReportFileNameDoesNotExist(fp->first, fileName)) {

					item = this->CreateItem();
					item->SetStatus(OvalEnum::STATUS_DOES_NOT_EXIST);
					item->AppendElement(new ItemEntity("filepath", "", OvalEnum::DATATYPE_STRING, OvalEnum::STATUS_DOES_NOT_EXIST));
					item->AppendElement(new ItemEntity("path", fp->first, OvalEnum::DATATYPE_STRING, OvalEnum::STATUS_EXISTS));
					item->AppendElement(new ItemEntity("filename", "", OvalEnum::DATATYPE_STRING, OvalEnum::STATUS_DOES_NOT_EXIST));
					ADD_WINDOWS_VIEW_ENTITY
					collectedItems->push_back(item);
					
				} else {

					item = this->CreateItem();
					item->SetStatus(OvalEnum::STATUS_EXISTS);
					item->AppendElement(new ItemEntity("path", fp->first, OvalEnum::DATATYPE_STRING, OvalEnum::STATUS_EXISTS));
					ADD_WINDOWS_VIEW_ENTITY
					collectedItems->push_back(item);

				}

			} else {
				Item *item;
				FS_REDIRECT_GUARD_BEGIN(fileFinder.GetView())
				item = this->EvaluateXpath((*iterator)->first, (*iterator)->second, xpath->GetValue());
				FS_REDIRECT_GUARD_END
				if(item != NULL) {
					ADD_WINDOWS_VIEW_ENTITY
					collectedItems->push_back(item);
				}
				item = NULL;

			}

			delete fp;
		}

	} else {
		if ( filePath != NULL ){
			StringVector fpaths;
			if (fileFinder.ReportFilePathDoesNotExist(filePath,&fpaths)){
				Item* item = NULL;
				StringPair* fpComponents = NULL;

				// build path ObjectEntity to pass to ReportPathDoesNotExist to retrieve the status of the path value
				ObjectEntity* pathStatus = new ObjectEntity("path","",OvalEnum::DATATYPE_STRING,OvalEnum::OPERATION_EQUALS,NULL,OvalEnum::CHECK_ALL,false);
				
				for(StringVector::iterator iterator = fpaths.begin(); iterator != fpaths.end(); iterator++) {
					item = this->CreateItem();
					item->SetStatus(OvalEnum::STATUS_DOES_NOT_EXIST);
					fpComponents = Common::SplitFilePath(*iterator);
					pathStatus->SetValue(fpComponents->first);
					item->AppendElement(new ItemEntity("filepath", "", OvalEnum::DATATYPE_STRING, OvalEnum::STATUS_DOES_NOT_EXIST));
					item->AppendElement(new ItemEntity("path", fpComponents->first, OvalEnum::DATATYPE_STRING, (fileFinder.ReportPathDoesNotExist(pathStatus))?OvalEnum::STATUS_DOES_NOT_EXIST:OvalEnum::STATUS_EXISTS));
					item->AppendElement(new ItemEntity("filename", "", OvalEnum::DATATYPE_STRING, OvalEnum::STATUS_DOES_NOT_EXIST));
					ADD_WINDOWS_VIEW_ENTITY
					collectedItems->push_back(item);
					
					if ( fpComponents != NULL ){
						delete fpComponents;
						fpComponents = NULL;
					}
				}

				if ( pathStatus != NULL ){
					delete pathStatus;
					pathStatus = NULL;
				}
			}
		}else{
			if(fileFinder.ReportPathDoesNotExist(path)) {
				Item* item = NULL;
				item = this->CreateItem();
				item->SetStatus(OvalEnum::STATUS_DOES_NOT_EXIST);
				item->AppendElement(new ItemEntity("path", "", OvalEnum::DATATYPE_STRING, OvalEnum::STATUS_DOES_NOT_EXIST));
				ADD_WINDOWS_VIEW_ENTITY
				collectedItems->push_back(item);
			}
		}
	}
	if ( filePaths != NULL ){
		delete filePaths;
		filePaths = NULL;
	}

	return collectedItems;
}

Item* XmlFileContentProbe::EvaluateXpath(string path, string fileName, string xpath) {

	Item* item = NULL;

	string contextNode = "/";

	string filePath = Common::BuildFilePath(path, fileName);

	XPathEvaluator::initialize();

	// Initialize the XalanSourceTree subsystem...
	XalanSourceTreeInit	theSourceTreeInit;

	// We'll use these to parse the XML file.
	XalanSourceTreeDOMSupport theDOMSupport;
	XalanSourceTreeParserLiaison theLiaison(theDOMSupport);
    theLiaison.setEntityResolver(new DummyEntityResolver());

	// Hook the two together...
	theDOMSupport.setParserLiaison(&theLiaison);

    try{				
		const XalanDOMString theFileName(filePath.c_str());

		// Create an input source that represents a local file...
		const LocalFileInputSource	theInputSource(theFileName.c_str());

		// Parse the document...
		XalanDocument* theDocument = NULL;
		try {
			theDocument = theLiaison.parseXMLStream(theInputSource);
		} catch (SAXParseException &e) {
			throw ProbeException("SAXParseException parsing " + filePath +
								 ": " + XmlCommon::ToString(e.getMessage()) + 
								 "  Line=" + Common::ToString(e.getLineNumber()) +
								 ", Col=" + Common::ToString(e.getColumnNumber()));
		} catch (SAXException &e) {
			throw ProbeException("SAXException parsing " + filePath + ": " +
								 XmlCommon::ToString(e.getMessage()));
		} catch(...) {
			theDocument = NULL;
			// this should never happen at this point only documents that exist should get here
		}

		if(theDocument == NULL) {
			throw ProbeException("Error: Unable to parse the current document: " + filePath);
		}else { 

		XalanDocumentPrefixResolver thePrefixResolver(theDocument);

		XPathEvaluator theEvaluator;

		// find the context node...
		XalanNode* const theContextNode = theEvaluator.selectSingleNode(  theDOMSupport,
																		  theDocument,
																		  XalanDOMString(contextNode.c_str()).c_str(),
																		  thePrefixResolver);

		if (theContextNode == 0) {
			throw ProbeException("Error the specified context node, \'" + contextNode + "\' was not found.");
		}else{
			
			// evaluate the expression...
			const XObjectPtr theResult = (
				theEvaluator.evaluate(
						theDOMSupport,
						theContextNode,
						XalanDOMString(xpath.c_str()).c_str(),
						thePrefixResolver));

			item = this->CreateItem();
			item->SetStatus(OvalEnum::STATUS_EXISTS);
			item->AppendElement(new ItemEntity("filepath", filePath, OvalEnum::DATATYPE_STRING));
			item->AppendElement(new ItemEntity("path", path, OvalEnum::DATATYPE_STRING, OvalEnum::STATUS_EXISTS));
			item->AppendElement(new ItemEntity("filename", fileName, OvalEnum::DATATYPE_STRING, OvalEnum::STATUS_EXISTS));
			item->AppendElement(new ItemEntity("xpath", xpath, OvalEnum::DATATYPE_STRING, OvalEnum::STATUS_EXISTS));
			
			switch ( ( theResult.get() )->getType() ){
				case XObject::eTypeBoolean:
				case XObject::eTypeNumber:
				case XObject::eTypeString:{
					
					string value;
					const CharVectorType chVec = TranscodeToLocalCodePage((theResult.get())->str());
					
					for( int i=0; chVec[i] !='\0'; i++)
						value += chVec[i]; 

					item->AppendElement(new ItemEntity("value_of", value, OvalEnum::DATATYPE_STRING, OvalEnum::STATUS_EXISTS));

					break;
				}
				case XObject::eTypeNodeSet:{
					
					NodeRefList list = (NodeRefList)theResult->nodeset();
					
					if ( list.getLength() <= 0 ){
						item->SetStatus(OvalEnum::STATUS_DOES_NOT_EXIST);
						item->AppendElement(new ItemEntity("value_of", "", OvalEnum::DATATYPE_STRING, OvalEnum::STATUS_DOES_NOT_EXIST));
					}

					for(unsigned int i = 0 ;  i < list.getLength() ; i++){
						XalanNode* node = list.item(i); 
						if(node->getNodeType() == XalanNode::TEXT_NODE) {
							const CharVectorType chVec = TranscodeToLocalCodePage(node->getNodeValue());
							string value;
					
							for( int i=0; chVec[i] !='\0'; i++)
								value += chVec[i]; 

							item->AppendElement(new ItemEntity("value_of", value, OvalEnum::DATATYPE_STRING, OvalEnum::STATUS_EXISTS));

						} else if(node->getNodeType() == XalanNode::ATTRIBUTE_NODE) {

							const CharVectorType chVec = TranscodeToLocalCodePage(node->getNodeValue());
							string value;

							for( int i=0; chVec[i] !='\0'; i++)
								value += chVec[i];

							item->AppendElement(new ItemEntity("value_of", value, OvalEnum::DATATYPE_STRING, OvalEnum::STATUS_EXISTS));
						} 
						
						else {
						
							item->SetStatus(OvalEnum::STATUS_DOES_NOT_EXIST);
							OvalMessage* m = new OvalMessage("An xpath expression is only allowed to select text nodes or attributes from a document.");
							item->AppendElement(new ItemEntity("value_of", "", OvalEnum::DATATYPE_STRING, OvalEnum::STATUS_DOES_NOT_EXIST));
							item->AppendMessage(m);
							throw ProbeException("Error: invalid xpath specified. An xpath expression is only allowed to select text nodes or attributes from a document.");
						}
					}
					break;
				}
				default:{
					item->SetStatus(OvalEnum::STATUS_DOES_NOT_EXIST);
					OvalMessage* m = new OvalMessage("An xpath object must be of type boolean, number, string, or node-set.");
					item->AppendElement(new ItemEntity("value_of", "", OvalEnum::DATATYPE_STRING, OvalEnum::STATUS_DOES_NOT_EXIST));
					item->AppendMessage(m);
					throw ProbeException("Error: invalid xpath object type was specified. An xpath object must be of type boolean, number, string, or node-set.");
				}
			}
			
		}
	}
	
	XPathEvaluator::terminate();

	} catch(const XSLException& theException) {
		
		// Convert the XSLException message to a string
		ostringstream m;
		m << (theException.getMessage());
		string errMsg = m.str();

		XPathEvaluator::terminate();

		throw ProbeException("Error while evaluating an xpath. " + errMsg);

	} catch(ProbeException ex) {

		XPathEvaluator::terminate();

		throw;

	} catch(Exception ex) {

		XPathEvaluator::terminate();

		throw;

	} catch(...) {

		XPathEvaluator::terminate();

		throw ProbeException("Error: XmlFileContentProbe() An unknown error occured while collecting data.");
	}

	return item;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  Private Members  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
Item* XmlFileContentProbe::CreateItem() {

	Item* item = new Item(0, 
						"http://oval.mitre.org/XMLSchema/oval-system-characteristics-5#independent", 
						"ind-sc", 
						"http://oval.mitre.org/XMLSchema/oval-system-characteristics-5#independent independent-system-characteristics-schema.xsd", 
						OvalEnum::STATUS_ERROR, 
						"xmlfilecontent_item");

	return item;
}


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ DummyEntityResolver methods ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
namespace {
	InputSource* DummyEntityResolver::resolveEntity(const XMLCh *const /*publicId*/, const XMLCh *const /*systemId*/)
	{
		return new DummyEntityResolver::NoOpInputSource();
	}

	BinInputStream* DummyEntityResolver::NoOpInputSource::makeStream() const
	{
		return new DummyEntityResolver::DoNothingBinInputStream();
	}

	XMLFilePos DummyEntityResolver::DoNothingBinInputStream::curPos() const
	{
		return 0;
	}

	XMLSize_t DummyEntityResolver::DoNothingBinInputStream::readBytes(XMLByte *const /*toFill*/, const XMLSize_t /*maxToRead*/)
	{
		return 0;
	}

	const XMLCh *DummyEntityResolver::DoNothingBinInputStream::getContentType() const
	{
		return NULL;
	}
}

