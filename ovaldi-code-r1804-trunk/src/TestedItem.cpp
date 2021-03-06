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

#include <xercesc/dom/DOMDocument.hpp>

#include "XmlCommon.h"
#include "Common.h"

#include "TestedItem.h"

using namespace xercesc;

//****************************************************************************************//
//								Test Class												  //	
//****************************************************************************************//
TestedItem::TestedItem() {

	this->SetResult(OvalEnum::RESULT_NOT_EVALUATED);
}

TestedItem::~TestedItem() {
	
}

// ***************************************************************************************	//
//								 Public members												//
// ***************************************************************************************	//

Item* TestedItem::GetItem() {

	return this->item;
}

void TestedItem::SetItem(Item* item) {

	this->item = item;
}

OvalEnum::ResultEnumeration TestedItem::GetResult() {

	return this->result;
}

void TestedItem::SetResult(OvalEnum::ResultEnumeration result) {

	this->result = result;
}

void TestedItem::Write(DOMElement* parentElm) {

	// get the parent document
	DOMDocument* resultDoc = parentElm->getOwnerDocument();

	// create a new tested_item element
	DOMElement* testedItemElm = XmlCommon::AddChildElementNS(resultDoc, parentElm, XmlCommon::resNS, "tested_item");

	// add the attributes
	XmlCommon::AddAttribute(testedItemElm, "item_id", Common::ToString(this->GetItem()->GetId()));
	XmlCommon::AddAttribute(testedItemElm, "result", OvalEnum::ResultToString(this->GetResult()));
}
