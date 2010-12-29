/*
 *  friendlyName.cpp
 *  CMSSW
 *
 *  Created by Chris Jones on 2/24/06.
 *
 */
#include <string>
#include <boost/regex.hpp>
#include <iostream>
#include <map>
#include "boost/thread/tss.hpp"

//NOTE:  This should probably be rewritten so that we break the class name into a tree where the template arguments are the node.  On the way down the tree
// we look for '<' or ',' and on the way up (caused by finding a '>') we can apply the transformation to the output string based on the class name for the
// templated class.  Up front we'd register a class name to a transformation function (which would probably take a std::vector<std::string> which holds
// the results of the node transformations)

namespace art {
  namespace friendlyname {
    static boost::regex const reBeginSpace("^ +");
    static boost::regex const reEndSpace(" +$");
    static boost::regex const reAllSpaces(" +");
    static boost::regex const reColons("::");
    static boost::regex const reComma(",");
    static boost::regex const reTemplateArgs("[^<]*<(.*)>$");
    static boost::regex const reTemplateClass("([^<>,]+<[^<>]*>)");
    static std::string const emptyString("");

    std::string handleNamespaces(std::string const& iIn) {
       return boost::regex_replace(iIn,reColons,emptyString,boost::format_perl);

    }

    std::string removeExtraSpaces(std::string const& iIn) {
       return boost::regex_replace(boost::regex_replace(iIn,reBeginSpace,emptyString),
                                    reEndSpace, emptyString);
    }

    std::string removeAllSpaces(std::string const& iIn) {
      return boost::regex_replace(iIn, reAllSpaces,emptyString);
    }
    static boost::regex const reWrapper("art::Wrapper<(.*)>");
    static boost::regex const reString("std::basic_string<char>");
    static boost::regex const reSorted("art::SortedCollection<(.*), *art::StrictWeakOrdering<\\1 *> >");
    static boost::regex const reUnsigned("unsigned ");
    static boost::regex const reLong("long ");
    static boost::regex const reVector("std::vector");
    static boost::regex const reAIKR(", *art::helper::AssociationIdenticalKeyReference"); //this is a default so can replaced with empty
    //force first argument to also be the argument to art::ClonePolicy so that if OwnVector is within
    // a template it will not eat all the remaining '>'s
    static boost::regex const reOwnVector("art::OwnVector<(.*), *art::ClonePolicy<\\1 *> >");

    //NOTE: the '?' means make the smallest match. This may lead to problems where the template arguments themselves have commas
    // but we are using it in the cases where art::AssociationMap appears multiple times in template arguments
    static boost::regex const reOneToOne("art::AssociationMap< *art::OneToOne<(.*?),(.*?), *u[a-z]*> >");
    static boost::regex const reOneToMany("art::AssociationMap< *art::OneToMany<(.*?),(.*?), *u[a-z]*> >");
    static boost::regex const reOneToValue("art::AssociationMap< *art::OneToValue<(.*?),(.*?), *u[a-z]*> >");
    static boost::regex const reOneToManyWithQuality("art::AssociationMap<art::OneToManyWithQuality<(.*?), *(.*?), *(.*?), *u[a-z]*> >");
    static boost::regex const reToVector("art::AssociationVector<(.*), *(.*), *art::Ref.*,.*>");
    //NOTE: if the item within a clone policy is a template, this substitution will probably fail
    static boost::regex const reToRangeMap("art::RangeMap< *(.*), *(.*), *art::ClonePolicy<([^>]*)> >");
    //NOTE: If container is a template with one argument which is its 'type' then can simplify name
    static boost::regex const reToRefs1("art::RefVector< *(.*)< *(.*) *>, *\\2 *, *art::refhelper::FindUsingAdvance< *\\1< *\\2 *> *, *\\2 *> *>");
    static boost::regex const reToRefs2("art::RefVector< *(.*) *, *(.*) *, *art::refhelper::FindUsingAdvance< *\\1, *\\2 *> *>");
    static boost::regex const reToRefsAssoc("art::RefVector< *Association(.*) *, *art::helper(.*), *Association(.*)::Find>");

    std::string standardRenames(std::string const& iIn) {
       using boost::regex_replace;
       using boost::regex;
       std::string name = regex_replace(iIn, reWrapper, "$1");
       name = regex_replace(name,reAIKR,"");
       name = regex_replace(name,reString,"String");
       name = regex_replace(name,reSorted,"sSorted<$1>");
       name = regex_replace(name,reUnsigned,"u");
       name = regex_replace(name,reLong,"l");
       name = regex_replace(name,reVector,"s");
       name = regex_replace(name,reOwnVector,"sOwned<$1>");
       name = regex_replace(name,reToVector,"AssociationVector<$1,To,$2>");
       name = regex_replace(name,reOneToOne,"Association<$1,ToOne,$2>");
       name = regex_replace(name,reOneToMany,"Association<$1,ToMany,$2>");
       name = regex_replace(name,reOneToValue,"Association<$1,ToValue,$2>");
       name = regex_replace(name,reOneToManyWithQuality,"Association<$1,ToMany,$2,WithQuantity,$3>");
       name = regex_replace(name,reToRangeMap,"RangeMap<$1,$2>");
       name = regex_replace(name,reToRefs1,"Refs<$1<$2>>");
       name = regex_replace(name,reToRefs2,"Refs<$1,$2>");
       name = regex_replace(name,reToRefsAssoc,"Refs<Association$1>");
       //std::cout <<"standardRenames '"<<name<<"'"<<std::endl;
       return name;
    }

    std::string handleTemplateArguments(std::string const&);
    std::string subFriendlyName(std::string const& iFullName) {
       using namespace boost;
       std::string result = removeExtraSpaces(iFullName);

       smatch theMatch;
       if(regex_match(result,theMatch,reTemplateArgs)) {
          //std::cout <<"found match \""<<theMatch.str(1) <<"\"" <<std::endl;
          //static regex const templateClosing(">$");
          //std::string aMatch = regex_replace(theMatch.str(1),templateClosing,"");
          std::string aMatch = theMatch.str(1);
          std::string theSub = handleTemplateArguments(aMatch);
          regex const eMatch(std::string("(^[^<]*)<")+aMatch+">");
          result = regex_replace(result,eMatch,theSub+"$1");
       }
       return result;
    }

    std::string handleTemplateArguments(std::string const& iIn) {
       using namespace boost;
       std::string result = removeExtraSpaces(iIn);
       bool shouldStop = false;
       while(!shouldStop) {
          if(std::string::npos != result.find_first_of("<")) {
             smatch theMatch;
             if(regex_search(result,theMatch,reTemplateClass)) {
                std::string templateClass = theMatch.str(1);
                std::string friendlierName = removeAllSpaces(subFriendlyName(templateClass));

                //std::cout <<" t: "<<templateClass <<" f:"<<friendlierName<<std::endl;
                result = regex_replace(result, regex(templateClass),friendlierName);
             } else {
                //static regex const eComma(",");
                //result = regex_replace(result,eComma,"");
                std::cout <<" no template match for \""<<result<<"\""<<std::endl;
                assert(0 =="failed to find a match for template class");
             }
          } else {
             shouldStop=true;
          }
       }
       result = regex_replace(result,reComma,"");
       return result;
    }
    std::string friendlyName(std::string const& iFullName) {
       typedef std::map<std::string, std::string> Map;
       static boost::thread_specific_ptr<Map> s_fillToFriendlyName;
       if(0 == s_fillToFriendlyName.get()){
          s_fillToFriendlyName.reset(new Map);
       }
       Map::const_iterator itFound = s_fillToFriendlyName->find(iFullName);
       if(s_fillToFriendlyName->end()==itFound) {
          itFound = s_fillToFriendlyName->insert(Map::value_type(iFullName, handleNamespaces(subFriendlyName(standardRenames(iFullName))))).first;
       }
       return itFound->second;
    }
  }
}  // art

