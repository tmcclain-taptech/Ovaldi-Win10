//
//
//****************************************************************************************//
// Copyright (c) 2002-2013, The MITRE Corporation
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

#include <Log.h>
#include <VectorPtrGuard.h>
#include "CommandReader.h"

#include "NvramProbe.h"

using namespace std;

//****************************************************************************************//
//								NvramProbe Class										  //
//****************************************************************************************//
NvramProbe *NvramProbe::instance = NULL;

NvramProbe::NvramProbe() : vars(NULL) {
}

NvramProbe::~NvramProbe() {
	NvramProbe::DeleteVars();
    instance = NULL;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  Public Members  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
AbsProbe* NvramProbe::Instance() {

	// Use lazy initialization
	if(instance == NULL)
		instance = new NvramProbe();

	return instance;
}

ItemVector* NvramProbe::CollectItems(Object *object) {

	// get the name from the provided object
	ObjectEntity* name = this->ValidateStringOperations(object->GetElementByName("nvram_var"));

	if (vars == NULL) {
		this->GetAllVars();
	}
	
	VectorPtrGuard<Item> collectedItems(new ItemVector());
	bool nameCheck = false;
	ItemEntity nameIe("nvram_var");
	// loop through all vars if they are a regex match on name create item and return it
	CachedVarsType::iterator iterator;
	for(iterator = vars->begin(); iterator != vars->end(); iterator++) {
		nameIe.SetValue(iterator->first);
		nameCheck = (name->Analyze(&nameIe) == OvalEnum::RESULT_TRUE);
		if (nameCheck) {
			collectedItems->push_back(this->FillItem(iterator->first, iterator->second));
		}
	}
	return collectedItems.release();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  Private Members  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
Item* NvramProbe::CreateItem() {

	Item* item = new Item(0, 
						"http://oval.mitre.org/XMLSchema/oval-system-characteristics-5#macos", 
						"macos-sc", 
						"http://oval.mitre.org/XMLSchema/oval-system-characteristics-5#macos macos-system-characteristics-schema.xsd", 
						OvalEnum::STATUS_ERROR, 
						"nvram_item");

	return item;
}

Item* NvramProbe::FillItem(const string &var, const string &value) {
	Item* item = this->CreateItem();
	item->SetStatus(OvalEnum::STATUS_EXISTS);
	item->AppendElement(new ItemEntity("nvram_var", var, OvalEnum::DATATYPE_STRING, OvalEnum::STATUS_EXISTS));
	item->AppendElement(new ItemEntity("nvram_value", value, OvalEnum::DATATYPE_STRING, OvalEnum::STATUS_EXISTS));
	return item;
}

ObjectEntity* NvramProbe::ValidateStringOperations(ObjectEntity* stringOp) {
	if (stringOp != NULL) {
		// check operation - only allow  equals, not equals, case-insensitive and pattern match
		if(stringOp->GetOperation() != OvalEnum::OPERATION_EQUALS && stringOp->GetOperation() != OvalEnum::OPERATION_PATTERN_MATCH 
			&& stringOp->GetOperation() != OvalEnum::OPERATION_NOT_EQUAL && stringOp->GetOperation() != OvalEnum::OPERATION_CASE_INSENSITIVE_EQUALS
			&& stringOp->GetOperation() != OvalEnum::OPERATION_CASE_INSENSITIVE_NOT_EQUAL) {
			throw ProbeException("Error: invalid operation specified on name. Found: " + OvalEnum::OperationToString(stringOp->GetOperation()));
		}

		// check datatypes - only allow string
		if(stringOp->GetDatatype() != OvalEnum::DATATYPE_STRING) {
			throw ProbeException("Error: invalid data type specified. Found: " + OvalEnum::DatatypeToString(stringOp->GetDatatype()));
		}
	}
	return stringOp;
}

void NvramProbe::GetAllVars() {
	vars = new CachedVarsType();

	StringVector lines = CommandReader::GetResponse("/usr/sbin/nvram -p");

	for (StringVector::iterator it = lines.begin(); it != lines.end(); it++) {
		StringVector elems = CommandReader::Split(*it, '\t');
		if (elems.size() > 1)
			vars->push_back(make_pair(elems[0], elems[1]));
		else
			Log::Debug("Unrecognized nvram output format: " + *it);
	}
}

void NvramProbe::DeleteVars() {
	if (vars) {
		delete vars;
		vars = NULL;
	}
}
