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

#include <Windows.h>
//#include <Winldap.h>
#include <LM.h>
#include <dsgetdc.h>
#include <activeds.h>
#include <Sddl.h>
#include <iomanip>

#include "Log.h"
#include "Common.h"
#include "WindowsCommon.h"

#include "ActiveDirectoryProbe.h"

using namespace std;

//****************************************************************************************//
//                              ActiveDirectoryProbe Class                                //
//****************************************************************************************//

const string ActiveDirectoryProbe::NAMING_CONTEXT_TYPE_DOMAIN = "domain";
const string ActiveDirectoryProbe::NAMING_CONTEXT_TYPE_CONFIGURATION = "configuration";
const string ActiveDirectoryProbe::NAMING_CONTEXT_TYPE_SCHEMA = "schema";
const string ActiveDirectoryProbe::GET_ALL_ATTRIBUTES = "GET_ALL_ATTRIBUTES";
const string ActiveDirectoryProbe::GET_ALL_DISTINGUISHED_NAMES = "GET_ALL_DISTINGUISHED_NAMES";
const string ActiveDirectoryProbe::OBJECT_EXISTS = "OBJECT_EXISTS";

#define DISTINGUISHED_NAME_ATTRIBUTE L"distinguishedName"
#define OBJECT_CLASS_ATTRIBUTE_A "objectClass"
#define OBJECT_CLASS_ATTRIBUTE L"objectClass"

const string ActiveDirectoryProbe::LDAP_PROTOCOL = "LDAP://";
ActiveDirectoryProbe* ActiveDirectoryProbe::instance = NULL;

ActiveDirectoryProbe::ActiveDirectoryProbe() {
    ActiveDirectoryProbe::distinguishedNames = NULL;
    activeDirectoryCache = new ActiveDirectoryMap();
    ActiveDirectoryProbe::GetDomainComponents();
}

ActiveDirectoryProbe::~ActiveDirectoryProbe() {
    instance = NULL;
    ActiveDirectoryProbe::DeleteDistinguishedNames();
    ActiveDirectoryProbe::DeleteActiveDirectoryCache();
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  Public Members  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

AbsProbe* ActiveDirectoryProbe::Instance() {
    // Use lazy initialization for instance of the shared resource probe
    if ( instance == NULL ) {
        instance = new ActiveDirectoryProbe();
    }

    return instance;
}

ItemVector* ActiveDirectoryProbe::CollectItems ( Object* object ) {
    // Get the naming_context, relative_dn, and attribute from the provided object
    ObjectEntity* namingContextEntity = object->GetElementByName ( "naming_context" );
    ObjectEntity* relativeDnEntity = object->GetElementByName ( "relative_dn" );
    ObjectEntity* attributeEntity = object->GetElementByName ( "attribute" );

    // Check data type - only allow the string data type
    if ( namingContextEntity->GetDatatype() != OvalEnum::DATATYPE_STRING ) {
        throw ProbeException ( "Error: Invalid data type specified on naming_context. Found: " + OvalEnum::DatatypeToString ( namingContextEntity->GetDatatype() ) );
    }

    // Check operation - only allow  equals, not equals and patternStr match
    if ( namingContextEntity->GetOperation() != OvalEnum::OPERATION_EQUALS && namingContextEntity->GetOperation() != OvalEnum::OPERATION_PATTERN_MATCH && namingContextEntity->GetOperation() != OvalEnum::OPERATION_NOT_EQUAL ) {
        throw ProbeException ( "Error: Invalid operation specified on naming_context. Found: " + OvalEnum::OperationToString ( namingContextEntity->GetOperation() ) );
    }

    // Check data type - only allow the string data type
    if ( relativeDnEntity->GetDatatype() != OvalEnum::DATATYPE_STRING ) {
        throw ProbeException ( "Error: Invalid data type specified on relative_dn. Found: " + OvalEnum::DatatypeToString ( relativeDnEntity->GetDatatype() ) );
    }

    // Check operation - only allow  equals, not equals and patternStr match
    if ( relativeDnEntity->GetOperation() != OvalEnum::OPERATION_EQUALS && relativeDnEntity->GetOperation() != OvalEnum::OPERATION_PATTERN_MATCH && relativeDnEntity->GetOperation() != OvalEnum::OPERATION_NOT_EQUAL ) {
        throw ProbeException ( "Error: Invalid operation specified on relative_dn. Found: " + OvalEnum::OperationToString ( relativeDnEntity->GetOperation() ) );
    }

    // Check data type - only allow the string data type
    if ( attributeEntity->GetDatatype() != OvalEnum::DATATYPE_STRING ) {
        throw ProbeException ( "Error: Invalid data type specified on attribute. Found: " + OvalEnum::DatatypeToString ( attributeEntity->GetDatatype() ) );
    }

    // Check operation - only allow  equals, not equals and patternStr match
    if ( attributeEntity->GetOperation() != OvalEnum::OPERATION_EQUALS && attributeEntity->GetOperation() != OvalEnum::OPERATION_PATTERN_MATCH && attributeEntity->GetOperation() != OvalEnum::OPERATION_NOT_EQUAL ) {
        throw ProbeException ( "Error: Invalid operation specified on attribute. Found: " + OvalEnum::OperationToString ( attributeEntity->GetOperation() ) );
    }

    ItemVector* collectedItems = new ItemVector();
    StringSet* namingContexts = this->GetNamingContexts ( namingContextEntity );

    if ( namingContexts->size() > 0 ) {
        for ( StringSet::iterator iterator1 = namingContexts->begin() ; iterator1 != namingContexts->end() ; iterator1++ ) {
            string namingContextStr = ( *iterator1 );
            StringSet* relativeDns = this->GetRelativeDns ( namingContextStr, relativeDnEntity );

            if ( relativeDns->size() > 0 ) {
                for ( StringSet::iterator iterator2 = relativeDns->begin() ; iterator2 != relativeDns->end() ; iterator2++ ) {
                    string relativeDnStr = ( *iterator2 );
                    StringSet* attributes = this->GetAttributes ( namingContextStr, relativeDnStr, attributeEntity );

                    if ( attributes->size() > 0 ) {
                        for ( StringSet::iterator iterator3 = attributes->begin() ; iterator3 != attributes->end() ; iterator3++ ) {
                            string attributeStr = ( *iterator3 );
                            Item* item = ActiveDirectoryProbe::GetActiveDirectoryData ( namingContextStr , relativeDnStr , attributeStr );

                            if ( item != NULL ) {
								ItemEntityVector* itemVector = item->GetElements();
								// Get an index to use for inserting elements into the collection.  If a naming_context element exists, insert new elements after it.
								int index = item->GetElementsByName("naming_context")->size();
								
								if (relativeDnEntity->GetNil()) {
									ItemEntityVector* relativednVector = item->GetElementsByName("relative_dn");
									if (relativednVector->size() == 1) {
										relativednVector->at(0)->SetNil(true);
										relativednVector->at(0)->SetStatus(OvalEnum::STATUS_NOT_COLLECTED);
									} else if (relativednVector->size() == 0) {
										itemVector->insert(itemVector->begin() + index, new ItemEntity ( "relative_dn" , "" , OvalEnum::DATATYPE_STRING , OvalEnum::STATUS_NOT_COLLECTED, true ));
									}
								}
								// Increment the index so that future items are inserted after the relative_dn item.
								index += item->GetElementsByName("relative_dn")->size();

								if (attributeEntity->GetNil()) {
									ItemEntityVector* attributeVector = item->GetElementsByName("attribute");
									if (attributeVector->size() == 1) {
										attributeVector->at(0)->SetNil(true);
										attributeVector->at(0)->SetStatus(OvalEnum::STATUS_NOT_COLLECTED);
									} else if (attributeVector->size() == 0) {
										itemVector->insert(itemVector->begin() + index, new ItemEntity ( "attribute" , "" , OvalEnum::DATATYPE_STRING , OvalEnum::STATUS_NOT_COLLECTED, true ));
									}
								}

                                collectedItems->push_back ( new Item ( *item ) );
                            }
                        }

                    } else {
                        string objectClassStr = ActiveDirectoryProbe::GetObjectClass ( namingContextStr, relativeDnStr );

                        if ( attributeEntity->GetOperation() == OvalEnum::OPERATION_EQUALS ) {
                            if ( attributeEntity->GetVarRef() == NULL ) {
                                Item* item = this->CreateItem();
                                item->SetStatus ( OvalEnum::STATUS_DOES_NOT_EXIST );
                                item->AppendElement ( new ItemEntity ( "naming_context" , namingContextStr , OvalEnum::DATATYPE_STRING , OvalEnum::STATUS_EXISTS ) );
                                item->AppendElement ( new ItemEntity ( "relative_dn" , relativeDnStr , OvalEnum::DATATYPE_STRING , OvalEnum::STATUS_EXISTS, relativeDnEntity->GetNil() ) );
								item->AppendElement ( new ItemEntity ( "attribute" , "", OvalEnum::DATATYPE_STRING , OvalEnum::STATUS_DOES_NOT_EXIST, attributeEntity->GetNil() ) );

                                if ( objectClassStr.compare ( "" ) == 0 ) {
                                    item->AppendElement ( new ItemEntity ( "object_class" , "" , OvalEnum::DATATYPE_STRING , OvalEnum::STATUS_DOES_NOT_EXIST ) );

                                } else {
                                    item->AppendElement ( new ItemEntity ( "object_class" , objectClassStr , OvalEnum::DATATYPE_STRING , OvalEnum::STATUS_EXISTS ) );
                                }

                                collectedItems->push_back ( new Item ( *item ) );

                            } else {
								VariableValueVector vals = attributeEntity->GetVarRef()->GetValues();
                                for ( VariableValueVector::iterator iterator = vals.begin() ; iterator != vals.end() ; iterator++ ) {
                                    Item* item = this->CreateItem();
                                    item->SetStatus ( OvalEnum::STATUS_DOES_NOT_EXIST );
                                    item->AppendElement ( new ItemEntity ( "naming_context" , namingContextStr , OvalEnum::DATATYPE_STRING , OvalEnum::STATUS_EXISTS ) );
                                    item->AppendElement ( new ItemEntity ( "relative_dn" , relativeDnStr , OvalEnum::DATATYPE_STRING , OvalEnum::STATUS_EXISTS, relativeDnEntity->GetNil() ) );
                                    item->AppendElement ( new ItemEntity ( "attribute" , "" , OvalEnum::DATATYPE_STRING , OvalEnum::STATUS_DOES_NOT_EXIST, attributeEntity->GetNil() ) );

                                    if ( objectClassStr.compare ( "" ) == 0 ) {
                                        item->AppendElement ( new ItemEntity ( "object_class" , "" , OvalEnum::DATATYPE_STRING , OvalEnum::STATUS_DOES_NOT_EXIST ) );

                                    } else {
                                        item->AppendElement ( new ItemEntity ( "object_class" , objectClassStr , OvalEnum::DATATYPE_STRING , OvalEnum::STATUS_EXISTS ) );
                                    }

                                    collectedItems->push_back ( new Item ( *item ) );
                                }
                            }
                        }
                    }

                    attributes->clear();
                    delete attributes;
                }

            } else {
                if ( relativeDnEntity->GetOperation() == OvalEnum::OPERATION_EQUALS ) {
                    if ( relativeDnEntity->GetVarRef() == NULL ) {
                        Item* item = this->CreateItem();
                        item->SetStatus ( OvalEnum::STATUS_DOES_NOT_EXIST );
                        item->AppendElement ( new ItemEntity ( "naming_context" , namingContextStr , OvalEnum::DATATYPE_STRING , OvalEnum::STATUS_EXISTS ) );
                        item->AppendElement ( new ItemEntity ( "relative_dn" , "" , OvalEnum::DATATYPE_STRING , OvalEnum::STATUS_DOES_NOT_EXIST, relativeDnEntity->GetNil() ) );
                        collectedItems->push_back ( item );

                    } else {
						VariableValueVector vals = relativeDnEntity->GetVarRef()->GetValues();
                        for ( VariableValueVector::iterator iterator = vals.begin() ; iterator != vals.end() ; iterator++ ) {
                            Item* item = this->CreateItem();
                            item->SetStatus ( OvalEnum::STATUS_DOES_NOT_EXIST );
                            item->AppendElement ( new ItemEntity ( "naming_context" , namingContextStr , OvalEnum::DATATYPE_STRING , OvalEnum::STATUS_EXISTS ) );
                            item->AppendElement ( new ItemEntity ( "relative_dn" , "" , OvalEnum::DATATYPE_STRING , OvalEnum::STATUS_DOES_NOT_EXIST, relativeDnEntity->GetNil() ) );
                            collectedItems->push_back ( item );
                        }
                    }
                }
            }

            relativeDns->clear();
            delete relativeDns;
        }

    } else {
        if ( namingContextEntity->GetOperation() == OvalEnum::OPERATION_EQUALS ) {
            if ( namingContextEntity->GetVarRef() == NULL ) {
                Item* item = this->CreateItem();
                item->SetStatus ( OvalEnum::STATUS_DOES_NOT_EXIST );
                item->AppendElement ( new ItemEntity ( "naming_context" , "" , OvalEnum::DATATYPE_STRING , OvalEnum::STATUS_DOES_NOT_EXIST ) );
                collectedItems->push_back ( item );

            } else {
				VariableValueVector vals = namingContextEntity->GetVarRef()->GetValues();
                for ( VariableValueVector::iterator iterator = vals.begin() ; iterator != vals.end() ; iterator++ ) {
                    Item* item = this->CreateItem();
                    item->SetStatus ( OvalEnum::STATUS_DOES_NOT_EXIST );
                    item->AppendElement ( new ItemEntity ( "naming_context" , "" , OvalEnum::DATATYPE_STRING , OvalEnum::STATUS_DOES_NOT_EXIST ) );
                    collectedItems->push_back ( item );
                }
            }
        }
    }

    namingContexts->clear();
    delete namingContexts;
    return collectedItems;
}




//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~  Private Members  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~//

Item* ActiveDirectoryProbe::CreateItem() {
    Item* item = new Item ( 0 ,
                            "http://oval.mitre.org/XMLSchema/oval-system-characteristics-5#windows" ,
                            "win-sc" ,
                            "http://oval.mitre.org/XMLSchema/oval-system-characteristics-5#windows windows-system-characteristics-schema.xsd" ,
                            OvalEnum::STATUS_ERROR ,
                            "activedirectory_item" );
    return item;
}

string ActiveDirectoryProbe::GetAdsType ( ADSTYPE adsType ) {
    switch ( adsType ) {
        case 0:
            return "ADSTYPE_INVALID";
        case 1:
            return "ADSTYPE_DN_STRING";
        case 2:
            return "ADSTYPE_CASE_EXACT_STRING";
        case 3:
            return "ADSTYPE_CASE_IGNORE_STRING";
        case 4:
            return "ADSTYPE_PRINTABLE_STRING";
        case 5:
            return "ADSTYPE_NUMERIC_STRING";
        case 6:
            return "ADSTYPE_BOOLEAN";
        case 7:
            return "ADSTYPE_INTEGER";
        case 8:
            return "ADSTYPE_OCTET_STRING";
        case 9:
            return "ADSTYPE_UTC_TIME";
        case 10:
            return "ADSTYPE_LARGE_INTEGER";
        case 11:
            return "ADSTYPE_PROV_SPECIFIC";
        case 12:
            return "ADSTYPE_OBJECT_CLASS";
        case 13:
            return "ADSTYPE_CASEIGNORE_LIST";
        case 14:
            return "ADSTYPE_OCTET_LIST";
        case 15:
            return "ADSTYPE_PATH";
        case 16:
            return "ADSTYPE_POSTALADDRESS";
        case 17:
            return "ADSTYPE_TIMESTAMP";
        case 18:
            return "ADSTYPE_BACKLINK";
        case 19:
            return "ADSTYPE_TYPEDNAME";
        case 20:
            return "ADSTYPE_HOLD";
        case 21:
            return "ADSTYPE_NETADDRESS";
        case 22:
            return "ADSTYPE_REPLICAPOINTER";
        case 23:
            return "ADSTYPE_FAXNUMBER";
        case 24:
            return "ADSTYPE_EMAIL";
        case 25:
            return "ADSTYPE_NT_SECURITY_DESCRIPTOR";
        case 26:
            return "ADSTYPE_UNKNOWN";
        case 27:
            return "ADSTYPE_DN_WITH_BINARY";
        case 28:
            return "ADSTYPE_DN_WITH_STRING";
        default:
            return "";
    }
}

Item* ActiveDirectoryProbe::GetActiveDirectoryData ( string namingContextStr , string relativeDnStr , string attributeStr ) {
    ActiveDirectoryMap::iterator cacheIterator;
    string distinguishedNameStr  = ActiveDirectoryProbe::BuildDistinguishedName ( namingContextStr, relativeDnStr );

    if ( ( cacheIterator = ActiveDirectoryProbe::activeDirectoryCache->find ( distinguishedNameStr ) ) != ActiveDirectoryProbe::activeDirectoryCache->end() ) {
        StringKeyedItemMap::iterator attributeIterator;

        if ( ( attributeIterator = ( ( cacheIterator->second )->find ( attributeStr ) ) ) != ( cacheIterator->second )->end() ) {
            return attributeIterator->second;
        }
    }

    if ( ActiveDirectoryProbe::QueryActiveDirectory ( namingContextStr, relativeDnStr, attributeStr , NULL ) ) {
        if ( ( cacheIterator = ActiveDirectoryProbe::activeDirectoryCache->find ( distinguishedNameStr ) ) != ActiveDirectoryProbe::activeDirectoryCache->end() ) {
            StringKeyedItemMap::iterator attIterator;

            if ( ( attIterator = ( ( cacheIterator->second )->find ( attributeStr ) ) ) != ( cacheIterator->second )->end() ) {
                return attIterator->second;
            }
        }
    }

    return NULL;
}

string ActiveDirectoryProbe::GetObjectClass ( string namingContextStr , string relativeDnStr ) {
    string objectClassStr = "";
    Item* item = NULL;

    if ( ( item = ActiveDirectoryProbe::GetActiveDirectoryData ( namingContextStr , relativeDnStr , "" ) ) != NULL ) {
        objectClassStr = item->GetElementByName ( "object_class" )->GetValue();
    }

    return objectClassStr;
}

string ActiveDirectoryProbe::BuildDistinguishedName ( string namingContextStr, string relativeDnStr ) {
    string distinguishedNameStr = relativeDnStr;

    if ( namingContextStr.compare ( NAMING_CONTEXT_TYPE_CONFIGURATION ) == 0 ) {
        distinguishedNameStr.append ( ",CN=Configuration," );

    } else if ( namingContextStr.compare ( NAMING_CONTEXT_TYPE_DOMAIN ) == 0 ) {
        distinguishedNameStr.append ( "," );

    } else if ( namingContextStr.compare ( NAMING_CONTEXT_TYPE_SCHEMA ) == 0 ) {
        distinguishedNameStr.append ( ",CN=Schema,CN=Configuration," );

    } else {
        throw ProbeException ( "Error: The naming context '" + namingContextStr + "' is unknown." );
    }

    if ( relativeDnStr.compare ( "" ) == 0 ) {
        distinguishedNameStr = distinguishedNameStr.substr ( 1, distinguishedNameStr.length() );
    }

    distinguishedNameStr.append ( domainName );
    return distinguishedNameStr;
}


void ActiveDirectoryProbe::GetDomainComponents() {
    DOMAIN_CONTROLLER_INFO* DomainControllerInfo = NULL;
    DWORD dReturn = 0L;
    ULONG dcFlags;
    dcFlags = DS_WRITABLE_REQUIRED | DS_DIRECTORY_SERVICE_REQUIRED | DS_RETURN_DNS_NAME;
    dReturn = DsGetDcName ( NULL,                   // ComputerName
                            NULL,                     // DomainName
                            NULL,                     // DomainGuid
                            NULL,                     // SiteName
                            dcFlags,                  // Flags
                            &DomainControllerInfo );  // DomainControllerInfo buffer

    if ( dReturn == ERROR_SUCCESS ) {
        string domainStr = DomainControllerInfo->DomainName;
        // Turn the domain into the domain components part of the distinguished name.  The dc
        // string is formed by breaking up the domain name at each period.
        string retDomainStr = "DC=";
        retDomainStr.append ( domainStr );
        unsigned int pos = retDomainStr.find ( "." );

        while ( pos != string::npos ) {
            retDomainStr.replace ( pos, 1, ",DC=" );
            pos = retDomainStr.find ( ".", pos );
        }

        // Free the DomainControllerInfo buffer.
        if ( NetApiBufferFree ( DomainControllerInfo ) != NERR_Success ) {
            throw ProbeException ( "Error: NetApiBufferFree() was unable to free the memory allocated for the DOMAIN_CONTROLLER_INFO structure. Microsoft System Error (" + Common::ToString ( GetLastError() ) + ") - " + WindowsCommon::GetErrorMessage ( GetLastError() ) );
        }

        ActiveDirectoryProbe::dnsName = domainStr;
        ActiveDirectoryProbe::domainName = retDomainStr;

    } else {
        throw ProbeException ( "Error: DsGetDcName() was unable to retrieve the name of the specified domain controller. Microsoft System Error (" + Common::ToString ( GetLastError() ) + ") - " + WindowsCommon::GetErrorMessage ( GetLastError() ) );
    }
}

string ActiveDirectoryProbe::RemoveDnBase ( string namingContextStr, string distinguishedNameStr ) {
    int pos = 0;

    if ( namingContextStr.compare ( NAMING_CONTEXT_TYPE_CONFIGURATION ) == 0 ) {
        pos = distinguishedNameStr.find ( "CN=Configuration" );

        if ( pos == -1 ) pos = distinguishedNameStr.find ( "cn=configuration" );

    } else if ( namingContextStr.compare ( NAMING_CONTEXT_TYPE_DOMAIN ) == 0 ) {
        pos = distinguishedNameStr.find ( "DC=" );

        if ( pos == -1 ) pos = distinguishedNameStr.find ( "dc=" );

    } else if ( namingContextStr.compare ( NAMING_CONTEXT_TYPE_SCHEMA ) == 0 ) {
        pos = distinguishedNameStr.find ( "CN=Schema" );

        if ( pos == -1 ) pos = distinguishedNameStr.find ( "cn=schema" );

    } else {
        throw ProbeException ( "Error: Cannot remove the base of the distinguished name because the naming context '" + namingContextStr + "' is unknown." );
    }

    if ( pos == - 1 ) {
        return "";

    } else {
        return distinguishedNameStr.substr ( 0, pos - 1 );
    }
}

StringSet* ActiveDirectoryProbe::GetNamingContexts ( ObjectEntity* namingContextEntity ) {
    StringSet* namingContexts = NULL;

    // Does this ObjectEntity use variables?
    if ( namingContextEntity->GetVarRef() == NULL ) {
        // Proceed based on operation
        if ( namingContextEntity->GetOperation() == OvalEnum::OPERATION_EQUALS ) {
            namingContexts = new StringSet();

            // If the naming context exists add it to the list
            if ( this->NamingContextExists ( namingContextEntity->GetValue() ) ) {
                namingContexts->insert ( namingContextEntity->GetValue() );
            }

        } else if ( namingContextEntity->GetOperation() == OvalEnum::OPERATION_NOT_EQUAL ) {
            namingContexts = this->GetMatchingNamingContexts ( namingContextEntity->GetValue() , false );

        } else if ( namingContextEntity->GetOperation() == OvalEnum::OPERATION_PATTERN_MATCH ) {
            namingContexts = this->GetMatchingNamingContexts ( namingContextEntity->GetValue() , true );
        }

    } else {
        namingContexts = new StringSet();
        // Get all naming contexts
        StringSet* allNamingContexts = new StringSet();

        if ( namingContextEntity->GetOperation() == OvalEnum::OPERATION_EQUALS ) {
            // In the case of equals simply loop through all the
            // variable values and add them to the set of all naming contexts
            // if they exist on the system
			VariableValueVector vals = namingContextEntity->GetVarRef()->GetValues();
            for ( VariableValueVector::iterator iterator = vals.begin() ; iterator != vals.end() ; iterator++ ) {
                if ( this->NamingContextExists ( iterator->GetValue() ) ) {
                    allNamingContexts->insert ( iterator->GetValue() );
                }
            }

        } else {
            allNamingContexts = this->GetMatchingNamingContexts ( ".*" , true );
        }

        // Loop through all naming contexts on the system
        // only keep naming contexts that match operation and value and var check
        ItemEntity* tmp = this->CreateItemEntity ( namingContextEntity );

        for ( StringSet::iterator it = allNamingContexts->begin() ; it != allNamingContexts->end() ; it++ ) {
            tmp->SetValue ( ( *it ) );

            if ( namingContextEntity->Analyze ( tmp ) == OvalEnum::RESULT_TRUE ) {
                namingContexts->insert ( ( *it ) );
            }
        }
    }

    return namingContexts;
}

StringSet* ActiveDirectoryProbe::GetMatchingNamingContexts ( string patternStr , bool isRegex ) {
    StringSet* namingContexts = new StringSet();

    if ( this->IsMatch ( patternStr , NAMING_CONTEXT_TYPE_DOMAIN , isRegex ) && ActiveDirectoryProbe::NamingContextExists ( NAMING_CONTEXT_TYPE_DOMAIN ) ) {
        namingContexts->insert ( NAMING_CONTEXT_TYPE_DOMAIN );
    }

    if ( this->IsMatch ( patternStr , NAMING_CONTEXT_TYPE_CONFIGURATION , isRegex ) && ActiveDirectoryProbe::NamingContextExists ( NAMING_CONTEXT_TYPE_CONFIGURATION ) ) {
        namingContexts->insert ( NAMING_CONTEXT_TYPE_CONFIGURATION );
    }

    if ( this->IsMatch ( patternStr , NAMING_CONTEXT_TYPE_SCHEMA , isRegex ) && ActiveDirectoryProbe::NamingContextExists ( NAMING_CONTEXT_TYPE_SCHEMA ) ) {
        namingContexts->insert ( NAMING_CONTEXT_TYPE_SCHEMA );
    }

    return namingContexts;
}

bool ActiveDirectoryProbe::NamingContextExists ( string namingContextStr ) {
    bool exists = false;

    if ( ( namingContextStr.compare ( NAMING_CONTEXT_TYPE_DOMAIN ) == 0 ) || ( namingContextStr.compare ( NAMING_CONTEXT_TYPE_CONFIGURATION ) == 0 ) || ( namingContextStr.compare ( NAMING_CONTEXT_TYPE_SCHEMA ) == 0 ) ) {
        exists = ActiveDirectoryProbe::QueryActiveDirectory ( namingContextStr, "", ActiveDirectoryProbe::OBJECT_EXISTS, NULL );
    }

    return exists;
}


StringSet* ActiveDirectoryProbe::GetRelativeDns ( string namingContextStr , ObjectEntity* relativeDnEntity ) {
    StringSet* relativeDns = NULL;

    if ( relativeDnEntity->GetNil() ) {
        relativeDns = new StringSet();
        relativeDns->insert ( "" );
        return relativeDns;
    }

    // Does this ObjectEntity use variables?
    if ( relativeDnEntity->GetVarRef() == NULL ) {
        // Proceed based on operation
        if ( relativeDnEntity->GetOperation() == OvalEnum::OPERATION_EQUALS ) {
            relativeDns = new StringSet();

            // If the relative distinguished name exists add it to the list
            if ( this->RelativeDnExists ( namingContextStr , relativeDnEntity->GetValue() ) ) {
                relativeDns->insert ( relativeDnEntity->GetValue() );
            }

        } else if ( relativeDnEntity->GetOperation() == OvalEnum::OPERATION_NOT_EQUAL ) {
            relativeDns = this->GetMatchingRelativeDns ( namingContextStr , relativeDnEntity->GetValue() , false );

        } else if ( relativeDnEntity->GetOperation() == OvalEnum::OPERATION_PATTERN_MATCH ) {
            relativeDns = this->GetMatchingRelativeDns ( namingContextStr , relativeDnEntity->GetValue() , true );
        }

    } else {
        relativeDns = new StringSet();
        // Get all relative distinguished names
        StringSet* allRelativeDns = new StringSet();

        if ( relativeDnEntity->GetOperation() == OvalEnum::OPERATION_EQUALS ) {
            // In the case of equals simply loop through all the
            // variable values and add them to the set of all relative distinguished names
            // if they exist on the system
			VariableValueVector vals = relativeDnEntity->GetVarRef()->GetValues();
            for ( VariableValueVector::iterator iterator = vals.begin() ; iterator != vals.end() ; iterator++ ) {
                if ( this->RelativeDnExists ( namingContextStr , iterator->GetValue() ) ) {
                    allRelativeDns->insert ( iterator->GetValue() );
                }
            }

        } else {
            allRelativeDns = this->GetMatchingRelativeDns ( namingContextStr , ".*" , true );
        }

        // Loop through all relative distinguished names on the system
        // only keep the relative distinguished names that match operation and value and var check
        ItemEntity* tmp = this->CreateItemEntity ( relativeDnEntity );

        for ( StringSet::iterator it = allRelativeDns->begin() ; it != allRelativeDns->end() ; it++ ) {
            tmp->SetValue ( ( *it ) );

            if ( relativeDnEntity->Analyze ( tmp ) == OvalEnum::RESULT_TRUE ) {
                relativeDns->insert ( ( *it ) );
            }
        }
    }

    return relativeDns;
}

StringSet* ActiveDirectoryProbe::GetMatchingRelativeDns ( string namingContextStr , string patternStr , bool isRegex ) {
    string relativeDistinguishedNameStr;
    string distinguishedNameStr;
    StringSet* relativeDns = new StringSet();

    if ( ActiveDirectoryProbe::distinguishedNames == NULL ) {
        StringSet* allDistinguishedNames = new StringSet();

        if ( ActiveDirectoryProbe::QueryActiveDirectory ( namingContextStr, patternStr, ActiveDirectoryProbe::GET_ALL_DISTINGUISHED_NAMES, allDistinguishedNames ) ) {
            ActiveDirectoryProbe::distinguishedNames = allDistinguishedNames;

        } else {
            allDistinguishedNames->clear();
            delete allDistinguishedNames;
        }
    }

    if ( ActiveDirectoryProbe::distinguishedNames != NULL ) {
        distinguishedNameStr = ActiveDirectoryProbe::BuildDistinguishedName ( namingContextStr, patternStr );

        for ( StringSet::iterator iterator = ActiveDirectoryProbe::distinguishedNames->begin() ; iterator != ActiveDirectoryProbe::distinguishedNames->end() ; iterator++ ) {
            if ( this->IsMatch (  distinguishedNameStr , ( *iterator ) , isRegex ) ) {
                relativeDistinguishedNameStr = ActiveDirectoryProbe::RemoveDnBase ( namingContextStr, *iterator );

                if ( ! ( relativeDistinguishedNameStr.compare ( "" ) == 0 ) ) {
                    relativeDns->insert ( relativeDistinguishedNameStr );
                }
            }
        }
    }

    return relativeDns;
}

bool ActiveDirectoryProbe::RelativeDnExists ( string namingContextStr , string relativeDnStr ) {
    bool exists = false;

    if ( ActiveDirectoryProbe::distinguishedNames == NULL ) {
        exists = ActiveDirectoryProbe::QueryActiveDirectory ( namingContextStr, relativeDnStr, ActiveDirectoryProbe::OBJECT_EXISTS, NULL );

    } else {
        if ( ActiveDirectoryProbe::distinguishedNames->find ( ActiveDirectoryProbe::BuildDistinguishedName ( namingContextStr, relativeDnStr ) ) != ActiveDirectoryProbe::distinguishedNames->end() ) {
            exists = true;
        }
    }

    return exists;
}


StringSet* ActiveDirectoryProbe::GetAttributes ( string namingContextStr , string relativeDnStr , ObjectEntity* attributeEntity ) {
    StringSet* attributes = NULL;

    if ( attributeEntity->GetNil() ) {
        attributes = new StringSet();
        attributes->insert ( "" );
        return attributes;
    }

    // Does this ObjectEntity use variables?
    if ( attributeEntity->GetVarRef() == NULL ) {
        // Proceed based on operation
        if ( attributeEntity->GetOperation() == OvalEnum::OPERATION_EQUALS ) {
            attributes = new StringSet();

            // If the attributes exists add it to the list
            if ( this->AttributeExists ( namingContextStr , relativeDnStr , attributeEntity->GetValue() ) ) {
                attributes->insert ( attributeEntity->GetValue() );
            }

        } else if ( attributeEntity->GetOperation() == OvalEnum::OPERATION_NOT_EQUAL ) {
            attributes = this->GetMatchingAttributes ( namingContextStr , relativeDnStr , attributeEntity->GetValue() , false );

        } else if ( attributeEntity->GetOperation() == OvalEnum::OPERATION_PATTERN_MATCH ) {
            attributes = this->GetMatchingAttributes ( namingContextStr , relativeDnStr , attributeEntity->GetValue() , true );
        }

    } else {
        attributes = new StringSet();
        // Get all attributes
        StringSet* allAttributes = new StringSet();

        if ( attributeEntity->GetOperation() == OvalEnum::OPERATION_EQUALS ) {
            // In the case of equals simply loop through all the
            // variable values and add them to the set of all attributes
            // if they exist on the system
			VariableValueVector vals = attributeEntity->GetVarRef()->GetValues();
            for ( VariableValueVector::iterator iterator = vals.begin() ; iterator != vals.end() ; iterator++ ) {
                if ( this->AttributeExists ( namingContextStr , relativeDnStr , iterator->GetValue() ) ) {
                    allAttributes->insert ( iterator->GetValue() );
                }
            }

        } else {
            allAttributes = this->GetMatchingAttributes ( namingContextStr , relativeDnStr , ".*" , true );
        }

        // Loop through all attributes on the system
        // only keep attributes that match operation and value and var check
        ItemEntity* tmp = this->CreateItemEntity ( attributeEntity );

        for ( StringSet::iterator it = allAttributes->begin() ; it != allAttributes->end() ; it++ ) {
            tmp->SetValue ( ( *it ) );

            if ( attributeEntity->Analyze ( tmp ) == OvalEnum::RESULT_TRUE ) {
                attributes->insert ( ( *it ) );
            }
        }
    }

    return attributes;
}

StringSet* ActiveDirectoryProbe::GetMatchingAttributes ( string namingContextStr , string relativeDnStr , string patternStr , bool isRegex ) {
    StringSet* allAttributes = new StringSet();
    StringSet* attributes = new StringSet();

    if ( ActiveDirectoryProbe::QueryActiveDirectory ( namingContextStr, relativeDnStr, ActiveDirectoryProbe::GET_ALL_ATTRIBUTES, allAttributes ) ) {
        for ( StringSet::iterator iterator = allAttributes->begin(); iterator != allAttributes->end() ; iterator++ ) {
            if ( this->IsMatch ( patternStr , *iterator , isRegex )  ) {
                attributes->insert ( *iterator );
            }
        }
    }

    allAttributes->clear();
    delete allAttributes;
    return attributes;
}

bool ActiveDirectoryProbe::AttributeExists ( string namingContextStr , string relativeDnStr , string attributeStr ) {
    bool exists = false;
    StringSet* allAttributes = new StringSet();

    if ( ActiveDirectoryProbe::QueryActiveDirectory ( namingContextStr, relativeDnStr, ActiveDirectoryProbe::GET_ALL_ATTRIBUTES, allAttributes ) && allAttributes->find ( attributeStr ) != allAttributes->end() ) {
        exists = true;
    }

    return exists;
}

bool ActiveDirectoryProbe::QueryActiveDirectory ( string namingContextStr , string relativeDnStr , string activeDirectoryOperationStr  , StringSet* activeDirectoryData ) {
    ADS_SEARCH_COLUMN adsiColumn;
    ADS_SEARCHPREF_INFO searchPreferences[2];
    ADS_SEARCH_HANDLE hSearch;
    HRESULT hResult;
    LPWSTR columnName;
    IDirectorySearch *adsiSearch = NULL;
    bool exists = false;
    string objectClassValue = "";
    string pathNameStr = LDAP_PROTOCOL;
    string distinguishedNameStr = ActiveDirectoryProbe::BuildDistinguishedName ( namingContextStr, relativeDnStr );

    if ( activeDirectoryOperationStr.compare ( ActiveDirectoryProbe::GET_ALL_DISTINGUISHED_NAMES ) == 0 ) {
        pathNameStr.append ( ActiveDirectoryProbe::dnsName );
        pathNameStr.append ( "/" );
        pathNameStr.append ( ActiveDirectoryProbe::domainName );

    } else {
        pathNameStr.append ( ActiveDirectoryProbe::dnsName );
        pathNameStr.append ( "/" );
        pathNameStr.append ( distinguishedNameStr );
    }
	wstring wPathNameStr = WindowsCommon::StringToWide ( pathNameStr );
    hResult = ADsOpenObject (wPathNameStr.c_str() , NULL , NULL , ADS_SECURE_AUTHENTICATION , IID_IDirectorySearch, ( void ** ) & adsiSearch );
    if ( SUCCEEDED ( hResult ) ) {
        if ( activeDirectoryOperationStr.compare ( ActiveDirectoryProbe::GET_ALL_ATTRIBUTES ) == 0 ) {
            searchPreferences[0].dwSearchPref = ADS_SEARCHPREF_PAGESIZE;
            searchPreferences[0].vValue.dwType = ADSTYPE_INTEGER;
            searchPreferences[0].vValue.Integer = MAXDWORD;
            searchPreferences[1].dwSearchPref = ADS_SEARCHPREF_SEARCH_SCOPE;
            searchPreferences[1].vValue.dwType = ADSTYPE_INTEGER;
            searchPreferences[1].vValue.Integer = ADS_SCOPE_BASE;
            hResult = adsiSearch->SetSearchPreference ( searchPreferences, 2 );
            hResult = adsiSearch->ExecuteSearch ( NULL, NULL, 0xFFFFFFFF, &hSearch );

            if ( SUCCEEDED ( hResult ) ) {
                while ( adsiSearch->GetNextRow ( hSearch ) != S_ADS_NOMORE_ROWS ) {
                    exists = true;

                    while ( ( hResult = adsiSearch->GetNextColumnName ( hSearch, &columnName ) ) == S_OK ) {
                        hResult = adsiSearch->GetColumn ( hSearch, columnName, &adsiColumn );

                        if ( SUCCEEDED ( hResult ) ) {
                            activeDirectoryData->insert ( WindowsCommon::UnicodeToAsciiString ( columnName ) );
                        }
                    }
                }
            }

        } else if ( activeDirectoryOperationStr.compare ( ActiveDirectoryProbe::GET_ALL_DISTINGUISHED_NAMES ) == 0 ) {

            searchPreferences[0].dwSearchPref = ADS_SEARCHPREF_PAGESIZE;
            searchPreferences[0].vValue.dwType = ADSTYPE_INTEGER;
            searchPreferences[0].vValue.Integer = MAXDWORD;
            hResult = adsiSearch->SetSearchPreference ( searchPreferences, 1 );

			LPWSTR attrNames[] = { DISTINGUISHED_NAME_ATTRIBUTE };

            hResult = adsiSearch->ExecuteSearch ( NULL, attrNames, 1, &hSearch );

            if ( SUCCEEDED ( hResult ) ) {
                while ( ( hResult = adsiSearch->GetNextRow ( hSearch ) ) != S_ADS_NOMORE_ROWS ) {
                    exists = true;
                    hResult = adsiSearch->GetColumn ( hSearch , DISTINGUISHED_NAME_ATTRIBUTE , &adsiColumn );

                    if ( SUCCEEDED ( hResult ) ) {

                        for ( unsigned int i = 0; i < adsiColumn.dwNumValues ; i++ ) {
                            if ( adsiColumn.dwADsType == ADSTYPE_DN_STRING ) {
                                activeDirectoryData->insert ( WindowsCommon::UnicodeToAsciiString ( adsiColumn.pADsValues[i].DNString ) );
                            }
                        }
                    }
                }
            }

        } else if ( activeDirectoryOperationStr.compare ( ActiveDirectoryProbe::OBJECT_EXISTS ) == 0 ) {
            searchPreferences[0].dwSearchPref = ADS_SEARCHPREF_SEARCH_SCOPE;
            searchPreferences[0].vValue.dwType = ADSTYPE_INTEGER;
            searchPreferences[0].vValue.Integer = ADS_SCOPE_BASE;
            hResult = adsiSearch->SetSearchPreference ( searchPreferences, 1 );
            hResult = adsiSearch->ExecuteSearch ( NULL, NULL, 0xFFFFFFFF, &hSearch );

            if ( SUCCEEDED ( hResult ) ) {
                if ( adsiSearch->GetFirstRow ( hSearch ) == S_OK ) {
                    exists = true;
                }
            }

        } else {

            searchPreferences[0].dwSearchPref = ADS_SEARCHPREF_SEARCH_SCOPE;
            searchPreferences[0].vValue.dwType = ADSTYPE_INTEGER;
            searchPreferences[0].vValue.Integer = ADS_SCOPE_BASE;
            hResult = adsiSearch->SetSearchPreference ( searchPreferences, 1 );
			LPWSTR attributeArray[] = {OBJECT_CLASS_ATTRIBUTE, NULL};

            if ( activeDirectoryOperationStr.compare ( "" ) == 0 ) {
                hResult = adsiSearch->ExecuteSearch ( NULL , attributeArray , 1 , &hSearch );

            } else {
				// references I've seen say this is ok.  The c_str() pointer is only
				// invalidated if the string is modified, which we aren't doing.
				attributeArray[1] = (LPWSTR)activeDirectoryOperationStr.c_str();
                hResult = adsiSearch->ExecuteSearch ( NULL , attributeArray , 2 , &hSearch );
            }

            if ( SUCCEEDED ( hResult ) ) {
                while ( adsiSearch->GetNextRow ( hSearch ) != S_ADS_NOMORE_ROWS ) {
                    exists = true;

                    while ( ( hResult = adsiSearch->GetNextColumnName ( hSearch, &columnName ) ) == S_OK ) {
                        hResult = adsiSearch->GetColumn ( hSearch, columnName, &adsiColumn );

                        if ( SUCCEEDED ( hResult ) ) {
                            StringVector* values = new StringVector();
                            string attributeAdsTypeStr = ActiveDirectoryProbe::GetAdsType ( adsiColumn.dwADsType );
                            OvalMessage* message = NULL;

                            for ( unsigned int i = 0; i < adsiColumn.dwNumValues ; i++ ) {
								if (!wcscmp(columnName, OBJECT_CLASS_ATTRIBUTE)) {
                                    objectClassValue.append ( WindowsCommon::UnicodeToAsciiString ( adsiColumn.pADsValues[i].CaseIgnoreString ) );
                                    objectClassValue.append ( ";" );
                                }

                                switch ( adsiColumn.dwADsType ) {
                                    case 0: {
                                        //ADSTYPE_INVALID
                                        message = new OvalMessage ( "Error: The adstype value '" + attributeAdsTypeStr + "' is invalid." );
                                        break;
                                    }
                                    case 1: {
                                        //ADSTYPE_DN_STRING
                                        values->push_back ( WindowsCommon::UnicodeToAsciiString ( adsiColumn.pADsValues[i].DNString ) );
                                        break;
                                    }
                                    case 2: {
                                        //ADSTYPE_CASE_EXACT_STRING
                                        values->push_back ( WindowsCommon::UnicodeToAsciiString ( adsiColumn.pADsValues[i].CaseExactString ) );
                                        break;
                                    }
                                    case 3: {
                                        //ADSTYPE_CASE_IGNORE_STRING
                                        values->push_back ( WindowsCommon::UnicodeToAsciiString ( adsiColumn.pADsValues[i].CaseIgnoreString ) );
                                        break;
                                    }
                                    case 4: {
                                        //ADSTYPE_PRINTABLE_STRING
                                        values->push_back ( WindowsCommon::UnicodeToAsciiString ( adsiColumn.pADsValues[i].PrintableString ) );
                                        break;
                                    }
                                    case 5: {
                                        //ADSTYPE_NUMERIC_STRING
                                        values->push_back ( WindowsCommon::UnicodeToAsciiString ( adsiColumn.pADsValues[i].NumericString ) );
                                        break;
                                    }
                                    case 6: {
                                        //ADSTYPE_BOOLEAN
                                        values->push_back ( Common::ToString ( adsiColumn.pADsValues[i].Boolean ) );
                                        break;
                                    }
                                    case 7: {
                                        //ADSTYPE_INTEGER
                                        values->push_back ( Common::ToString ( adsiColumn.pADsValues[i].Integer ) );
                                        break;
                                    }
                                    case 8: {
                                        //ADSTYPE_OCTET_STRING
                                        string octetStr = ActiveDirectoryProbe::ConvertOctetString ( columnName, &adsiColumn.pADsValues[i].OctetString );

                                        if ( octetStr.compare ( "" ) == 0 ) {
                                            message = new OvalMessage ( "Error: The adstype value '" + attributeAdsTypeStr + "' is currently only supported for the objectSid and objectGUID attributes." );

                                        } else {
                                            values->push_back ( octetStr );
                                        }

                                        break;
                                    }
                                    case 9: {
                                        //ADSTYPE_UTC_TIME
                                        values->push_back ( ActiveDirectoryProbe::BuildUTCTimeString ( &adsiColumn.pADsValues[i].UTCTime ) );
                                        break;
                                    }
                                    case 10: {
                                        //ADSTYPE_LARGE_INTEGER
                                        values->push_back ( Common::ToString ( ( long long ) adsiColumn.pADsValues[i].LargeInteger.QuadPart ) );
                                        break;
                                    }
                                    case 11: {
                                        //ADSTYPE_PROV_SPECIFIC
                                        message = new OvalMessage ( "Error: The adstype value '" + attributeAdsTypeStr + "' is currently not supported." );
                                        break;
                                    }
                                    case 12: {
                                        //ADSTYPE_OBJECT_CLASS
                                        message = new OvalMessage ( "Error: The adstype value '" + attributeAdsTypeStr + "' is currently not supported." );
                                        break;
                                    }
                                    case 13: {
                                        //ADSTYPE_CASEIGNORE_LIST
                                        message = new OvalMessage ( "Error: The adstype value '" + attributeAdsTypeStr + "' is currently not supported." );
                                        break;
                                    }
                                    case 14: {
                                        //ADSTYPE_OCTET_LIST
                                        message = new OvalMessage ( "Error: The adstype value '" + attributeAdsTypeStr + "' is currently not supported." );
                                        break;
                                    }
                                    case 15: {
                                        //ADSTYPE_PATH
                                        values->push_back ( WindowsCommon::UnicodeToAsciiString ( ( adsiColumn.pADsValues[i].pPath )->Path ) );
                                        break;
                                    }
                                    case 16: {
                                        //ADSTYPE_POSTALADDRESS
                                        message = new OvalMessage ( "Error: The adstype value '" + attributeAdsTypeStr + "' is currently not supported." );
                                        break;
                                    }
                                    case 17: {
                                        //ADSTYPE_TIMESTAMP
                                        message = new OvalMessage ( "Error: The adstype value '" + attributeAdsTypeStr + "' is currently not supported." );
                                        break;
                                    }
                                    case 18: {
                                        //ADSTYPE_BACKLINK
                                        message = new OvalMessage ( "Error: The adstype value '" + attributeAdsTypeStr + "' is currently not supported." );
                                        break;
                                    }
                                    case 19: {
                                        //ADSTYPE_TYPEDNAME
                                        values->push_back ( WindowsCommon::UnicodeToAsciiString ( ( adsiColumn.pADsValues[i].pTypedName )->ObjectName ) );
                                        break;
                                    }
                                    case 20: {
                                        //ADSTYPE_HOLD
                                        message = new OvalMessage ( "Error: The adstype value '" + attributeAdsTypeStr + "' is currently not supported." );
                                        break;
                                    }
                                    case 21: {
                                        //ADSTYPE_NETADDRESS
                                        message = new OvalMessage ( "Error: The adstype value '" + attributeAdsTypeStr + "' is currently not supported." );
                                        break;
                                    }
                                    case 22: {
                                        //ADSTYPE_REPLICAPOINTER
                                        message = new OvalMessage ( "Error: The adstype value '" + attributeAdsTypeStr + "' is currently not supported." );
                                        break;
                                    }
                                    case 23: {
                                        //ADSTYPE_FAXNUMBER
                                        message = new OvalMessage ( "Error: The adstype value '" + attributeAdsTypeStr + "' is currently not supported." );
                                        break;
                                    }
                                    case 24: {
                                        //ADSTYPE_EMAIL
                                        message = new OvalMessage ( "Error: The adstype value '" + attributeAdsTypeStr + "' is currently not supported." );
                                        break;
                                    }
                                    case 25: {
                                        //ADSTYPE_NT_SECURITY_DESCRIPTOR
                                        message = new OvalMessage ( "Error: The adstype value '" + attributeAdsTypeStr + "' is currently not supported." );
                                        break;
                                    }
                                    case 26: {
                                        //ADSTYPE_UNKNOWN
                                        message = new OvalMessage ( "Error: The adstype value '" + attributeAdsTypeStr + "' is currently not supported." );
                                        break;
                                    }
                                    case 27: {
                                        //ADSTYPE_DN_WITH_BINARY
                                        message = new OvalMessage ( "Error: The adstype value '" + attributeAdsTypeStr + "' is currently not supported." );
                                        break;
                                    }
                                    case 28: {
                                        //ADSTYPE_DN_WITH_STRING
                                        message = new OvalMessage ( "Error: The adstype value '" + attributeAdsTypeStr + "' is currently not supported." );
                                        break;
                                    }
                                    default: {
                                        throw ProbeException ( "Error: The adstype value is unknown.\n" );
                                        break;
                                    }
                                }
                            } // for ( unsigned int i = 0; i < adsiColumn.dwNumValues ..

                            if ( wcscmp(columnName, OBJECT_CLASS_ATTRIBUTE ) != 0 || activeDirectoryOperationStr.compare ( OBJECT_CLASS_ATTRIBUTE_A ) == 0 || activeDirectoryOperationStr.empty() ) {
                                Item* item = this->CreateItem();
                                item->SetStatus ( OvalEnum::STATUS_EXISTS );
                                item->AppendElement ( new ItemEntity ( "naming_context" , namingContextStr , OvalEnum::DATATYPE_STRING , OvalEnum::STATUS_EXISTS ) );

								string attributeStr = WindowsCommon::UnicodeToAsciiString(columnName);

								if ( ! ( relativeDnStr.compare ( "" ) == 0 ) ) {
                                    item->AppendElement ( new ItemEntity ( "relative_dn" , relativeDnStr , OvalEnum::DATATYPE_STRING , OvalEnum::STATUS_EXISTS ) );
                                }

                                if ( activeDirectoryOperationStr.compare ( "" ) == 0 ) {
                                    item->AppendElement ( new ItemEntity ( "object_class" , objectClassValue , OvalEnum::DATATYPE_STRING , OvalEnum::STATUS_EXISTS ) );
                                    attributeStr = "";

                                } else {
                                    item->AppendElement ( new ItemEntity ( "attribute" , attributeStr , OvalEnum::DATATYPE_STRING , OvalEnum::STATUS_EXISTS ) );
                                    item->AppendElement ( new ItemEntity ( "object_class" , objectClassValue , OvalEnum::DATATYPE_STRING , OvalEnum::STATUS_EXISTS ) );
                                    item->AppendElement ( new ItemEntity ( "adstype" , attributeAdsTypeStr , OvalEnum::DATATYPE_STRING , OvalEnum::STATUS_EXISTS ) );

                                    for ( StringVector::iterator it = values->begin(); it != values->end(); it++ ) {
                                        item->AppendElement ( new ItemEntity ( "value" , *it , OvalEnum::DATATYPE_STRING , OvalEnum::STATUS_EXISTS ) );
                                    }
                                }

                                if ( message != NULL ) {
                                    message->SetLevel ( OvalEnum::LEVEL_ERROR );
                                    item->AppendMessage ( message );
                                    item->SetStatus ( OvalEnum::STATUS_ERROR );
                                }

                                if ( ActiveDirectoryProbe::activeDirectoryCache->find ( distinguishedNameStr ) == ActiveDirectoryProbe::activeDirectoryCache->end() ) {
                                    StringKeyedItemMap* attributeMap = new StringKeyedItemMap();
                                    attributeMap->insert ( make_pair ( attributeStr, item ) );

                                    if ( attributeMap->size() > 0 ) {
                                        ActiveDirectoryProbe::activeDirectoryCache->insert ( make_pair ( distinguishedNameStr, attributeMap ) );
                                    }

                                } else {
                                    ActiveDirectoryProbe::activeDirectoryCache->find ( distinguishedNameStr )->second->insert ( make_pair ( attributeStr, item ) );
                                }
                            }

                            values->clear();
                            delete values;
                            adsiSearch->FreeColumn ( &adsiColumn );
                        } // if ( SUCCEEDED ( hResult ) )
                    } // while ( ( hResult = adsiSearch->GetNextColumnName ...
                } // while ( adsiSearch->GetNextRow ...
            } // if ( SUCCEEDED ( hResult ) )
        }

        adsiSearch->CloseSearchHandle ( hSearch );
        adsiSearch->Release();

    } else {
        string message = "";
        message.append ( "Warning: The ADSI object " );
        message.append ( distinguishedNameStr );
        message.append ( " could not be opened. " );
        message.append ( ActiveDirectoryProbe::GetLastAdsiErrorMessage ( hResult ) );
        Log::Debug ( message );
    }

    return exists;
}

string ActiveDirectoryProbe::BuildUTCTimeString ( SYSTEMTIME* time ) {
    string timeStr = "";
    timeStr.append ( Common::ToString ( time->wMonth ) );
    timeStr.append ( "/" );
    timeStr.append ( Common::ToString ( time->wDay ) );
    timeStr.append ( "/" );
    timeStr.append ( Common::ToString ( time->wYear ) );
    timeStr.append ( " " );
    timeStr.append ( Common::ToString ( time->wHour ) );
    timeStr.append ( ":" );
    timeStr.append ( Common::ToString ( time->wMinute ) );
    timeStr.append ( ":" );
    timeStr.append ( Common::ToString ( time->wSecond ) );
    return timeStr;
}
string ActiveDirectoryProbe::ConvertOctetString ( LPCWSTR attributeStr , ADS_OCTET_STRING* octetString ) {
    string octetStr = "";
    PSID objectSID = NULL;
    LPOLESTR sid = NULL;
    LPOLESTR guid = new WCHAR [39];
    LPGUID objectGUID = NULL;

    if ( wcscmp(attributeStr, L"objectSid" ) == 0 ) {
        objectSID = ( PSID ) ( octetString->lpValue );
        ConvertSidToStringSid ( objectSID, ( LPSTR* ) &sid );
        octetStr = ( LPSTR ) sid;
        LocalFree ( sid );

    } else if ( wcscmp(attributeStr, L"objectGUID" ) == 0 ) {
        objectGUID = ( LPGUID ) ( octetString->lpValue );
        StringFromGUID2 ( *objectGUID, guid, 39 );
        octetStr = WindowsCommon::UnicodeToAsciiString ( ( wchar_t* ) guid );
        octetStr = octetStr.substr ( 1, octetStr.size() - 2 );

    } else {
        return octetStr = "";
    }

    return octetStr;
}
string ActiveDirectoryProbe::GetLastAdsiErrorMessage ( HRESULT hResult ) {
    string message = "";

    if ( hResult & 0x00005000 ) {
        ostringstream os;
        os << "0x" << std::hex << std::setfill ( '0' ) << std::setw ( 8 ) << hResult ;
        message.append ( "Microsoft ADSI Error (" );
        message.append ( os.str() );
        message.append ( ") - " );

        switch ( hResult ) {
            case 0x00005011:
                message.append ( "During a query, one or more errors occurred." );
                break;
            case 0x00005012:
                message.append ( "The search operation has reached the last row." );
                break;
            case 0x00005013:
                message.append ( "The search operation has reached the last column for the current row." );
                break;
            case 0x80005000:
                message.append ( "An invalid ADSI pathname was passed." );
                break;
            case 0x80005001:
                message.append ( "An unknown ADSI domain object was requested." );
                break;
            case 0x80005002:
                message.append ( "An unknown ADSI user object was requested." );
                break;
            case 0x80005003:
                message.append ( "An unknown ADSI computer object was requested." );
                break;
            case 0x80005004:
                message.append ( "An unknown ADSI object was requested." );
                break;
            case 0x80005005:
                message.append ( "The specified ADSI property was not set." );
                break;
            case 0x80005006:
                message.append ( "The specified ADSI property is not supported." );
                break;
            case 0x80005007:
                message.append ( "The specified ADSI property is invalid." );
                break;
            case 0x80005008:
                message.append ( "One or more input parameters are invalid." );
                break;
            case 0x80005009:
                message.append ( "The specified ADSI object is not bound to a remote resource." );
                break;
            case 0x8000500A:
                message.append ( "The specified ADSI object has not been modified." );
                break;
            case 0x8000500B:
                message.append ( "The specified ADSI object has been modified." );
                break;
            case 0x8000500C:
                message.append ( "The data type cannot be converted to/from a native DS data type." );
                break;
            case 0x8000500D:
                message.append ( "The property cannot be found in the cache." );
                break;
            case 0x8000500E:
                message.append ( "The ADSI object exists." );
                break;
            case 0x8000500F:
                message.append ( "The attempted action violates the directory service schema rules." );
                break;
            case 0x80005010:
                message.append ( "The specified column in the ADSI was not set." );
                break;
            case 0x80005014:
                message.append ( "The specified search filter is invalid." );
                break;
            default:
                message.append ( "An unknown ADSI error has occurred." );
                break;
        }

    } else if ( HRESULT_FACILITY ( hResult ) == FACILITY_WIN32 ) {
        message.append ( "Microsoft System Error (" );
        message.append ( Common::ToString ( GetLastError() ) );
        message.append ( ") - " );
        message.append ( WindowsCommon::GetErrorMessage ( hResult ) );

    } else {
        DWORD errorCode;
        WCHAR errorMessage[MAX_PATH];
        WCHAR nameProvider[MAX_PATH];
        HRESULT hResult = ADsGetLastError ( &errorCode, errorMessage, MAX_PATH, nameProvider, MAX_PATH );

        if ( SUCCEEDED ( hResult ) ) {
            message.append ( "Microsoft Extended ADSI Error (" );
            message.append ( Common::ToString ( errorCode ) );
            message.append ( ") - " );
            message.append ( WindowsCommon::UnicodeToAsciiString ( ( wchar_t* ) errorMessage ) );
            message.append ( "." );

        } else {
            message.append ( "There was an error retrieving the information about the last extended ADSI error." );
        }
    }

    return message;
}

void ActiveDirectoryProbe::DeleteDistinguishedNames() {
    if ( ActiveDirectoryProbe::distinguishedNames != NULL ) {
        ActiveDirectoryProbe::distinguishedNames->clear();
        delete ActiveDirectoryProbe::distinguishedNames;
    }
}

void ActiveDirectoryProbe::DeleteActiveDirectoryCache() {
    for ( ActiveDirectoryMap::iterator activeDirectoryMapIter = ActiveDirectoryProbe::activeDirectoryCache->begin() ; activeDirectoryMapIter != ActiveDirectoryProbe::activeDirectoryCache->end() ; activeDirectoryMapIter++ ) {
        for ( StringKeyedItemMap::iterator attributeMapIter = activeDirectoryMapIter->second->begin() ; attributeMapIter != activeDirectoryMapIter->second->end() ; attributeMapIter++ ) {
            delete attributeMapIter->second;
        }

        delete activeDirectoryMapIter->second;
    }

    delete ActiveDirectoryProbe::activeDirectoryCache;
    ActiveDirectoryProbe::activeDirectoryCache = NULL;
}