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

#include <set>

#include <Log.h>

//	include the probe classes
#include "FileProbe.h"
#include "FileMd5Probe.h"
#include "FileHashProbe.h"
#include "FileHash58Probe.h"
#include "FamilyProbe.h"
#include "UnameProbe.h"
#include "ProcessProbe.h"
#include "Process58Probe.h"
#include "EnvironmentVariableProbe.h"
#include "EnvironmentVariable58Probe.h"
#include "TextFileContentProbe.h"
#include "XmlFileContentProbe.h"
#include "VariableProbe.h"
#include "InetdProbe.h"
#include "PasswordProbe.h"
#include "ShadowProbe.h"
#include "InterfaceProbe.h"
//#include "LDAPProbe.h"
#include "TextFileContent54Probe.h"
#include "XinetdProbe.h"
#include "RunLevelProbe.h"
// SOLARIS PORT NOTICE: Add other probes here to support collection of solaris specific objects.
#include "IsainfoProbe.h"
#include "Patch54Probe.h"

#include "ProbeFactory.h"

using namespace std;

typedef set<AbsProbe*> AbsProbeSet;
static AbsProbeSet _probes;

// ***************************************************************************************	//
//								 Public members												//
// ***************************************************************************************	//
AbsProbe* ProbeFactory::GetProbe(string objectName) {
	// -----------------------------------------------------------------------
	//	Abstract
	//
	//	Based on the provided object return the appropriate probe.
	//	If a Probe is not found return null
	// -----------------------------------------------------------------------

	AbsProbe* probe = NULL;

// here are the objects defined in the independent schema
	if(objectName.compare("family_object") == 0) {
		probe = FamilyProbe::Instance();
	} else if(objectName.compare("filemd5_object") == 0) {
		probe = FileMd5Probe::Instance();
	} else if(objectName.compare("filehash_object") == 0) {
		probe = FileHashProbe::Instance();
	} else if(objectName.compare("filehash58_object") == 0) {
		probe = FileHash58Probe::Instance();
	} else if(objectName.compare("environmentvariable_object") == 0) {
		probe = EnvironmentVariableProbe::Instance();
	} else if(objectName.compare("environmentvariable58_object") == 0) {
		probe = EnvironmentVariable58Probe::Instance();
	} else if(objectName.compare("variable_object") == 0) {
		probe = VariableProbe::Instance();
	} else if(objectName.compare("textfilecontent_object") == 0) {
		probe = TextFileContentProbe::Instance();
	} else if(objectName.compare("textfilecontent54_object") == 0) {
		probe = TextFileContent54Probe::Instance();
	} else if(objectName.compare("xmlfilecontent_object") == 0) {
		probe = XmlFileContentProbe::Instance();
		//	} else if(objectName.compare("ldap_object") == 0) {
		// Not currently implemented for solaris systems

// here are the objects defined in the unix schema
	} else if(objectName.compare("file_object") == 0) {
		probe = FileProbe::Instance();	
	} else if(objectName.compare("inetd_object") == 0) {
		probe = InetdProbe::Instance();
	} else if(objectName.compare("interface_object") == 0) {
		probe = InterfaceProbe::Instance();
	} else if(objectName.compare("password_object") == 0) {
		probe = PasswordProbe::Instance();
	} else if(objectName.compare("process_object") == 0) {
		probe = ProcessProbe::Instance();
	} else if(objectName.compare("process58_object") == 0) {
		probe = Process58Probe::Instance();
	} else if(objectName.compare("runlevel_object") == 0) {
		probe = RunLevelProbe::Instance();
		//	} else if(objectName.compare("sccs_object") == 0) {
		// Not currently implemented for any unix systems
	} else if(objectName.compare("shadow_object") == 0) {
		probe = ShadowProbe::Instance();
	} else if(objectName.compare("uname_object") == 0) {
		probe = UnameProbe::Instance();
	} else if(objectName.compare("xinetd_object") == 0) {
		probe = XinetdProbe::Instance();

// here are the solaris specific objects
	} else if(objectName.compare("isainfo_object") == 0) {
		probe = IsainfoProbe::Instance();
	} else if(objectName.compare("patch54_object") == 0) {
		probe = Patch54Probe::Instance();
/*	} else if(objectName.compare("package_object") == 0) {
		// SOLARIS PORT NOTICE: get an instance of the probe for this object
		*/
	} else {
		Log::Info(objectName + " is not currently supported.");
	}

  _probes.insert( probe );

	return probe;
}

void ProbeFactory::Shutdown() {
	// -----------------------------------------------------------------------
	//	Abstract
	//
	//	Shutdown all probes
	// -----------------------------------------------------------------------

  for( AbsProbeSet::iterator iter = _probes.begin(); iter != _probes.end(); ){
    delete (*iter);  // the probe better set it's instance pointer to NULL inside of its destructor
    _probes.erase( iter++ );
  }
}
