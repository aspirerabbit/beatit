
#ifndef SRC_UTIL_IO_IO_HPP_
#define SRC_UTIL_IO_IO_HPP_

#include <iostream>
#include <vector>
#include <fstream>
#include <string>


#include <boost/config/warning_disable.hpp>
#include <boost/spirit/include/qi.hpp>
#include "boost/function.hpp"
#include "boost/bind.hpp"
#include <boost/spirit/include/qi_symbols.hpp>
#include "libmesh/parallel.h"

namespace libMesh
{
namespace Parallel
{
class Communicator;
}
}

class GetPot;


namespace BeatIt
{

void printBanner( std::ostream& cout);

void saveData( double time,
		       std::vector<double>& var,
			   std::ostream& output);


template <class Container,  class ElementParser =  boost::spirit::qi::auto_type >
bool  readList(std::string& list, Container& container, const ElementParser& elementParser = ElementParser() )
{
	auto beg = list.begin();
	auto end = list.end();
	namespace qi = boost::spirit::qi;
	bool ok = qi::phrase_parse(beg, end,  elementParser  % ( ',' | qi::lit('\t') ) ,  qi::blank, container);
	return ok;
}

bool  readList(std::string& list, std::vector<std::string>& container );

void createOutputFolder(const libMesh::Parallel::Communicator & comm,
		                                          std::string& output_folder );

GetPot readInputFile( int argc, char ** argv);

} // namespace BeatIt


#endif /* SRC_UTIL_IO_IO_HPP_ */
