#include "common.h"
#include "parser.h"
#include <iostream>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/CompilerOutputter.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>

class ParserTest : public CPPUNIT_NS::TestFixture {
  CPPUNIT_TEST_SUITE(ParserTest);
  CPPUNIT_TEST(testSimpleParse);
  CPPUNIT_TEST(testMapVsIndex);
//  CPPUNIT_TEST(testCallVsField);
  CPPUNIT_TEST_SUITE_END();

public:
  void setUp() {}
  void tearDown() {}
  void testSimpleParse() {
    std::vector<PTR<cirth::ast::Expression> > exps;
    CPPUNIT_ASSERT(cirth::parser::parse("hey. there", exps));
    CPPUNIT_ASSERT(exps.size() == 1);
    CPPUNIT_ASSERT(exps[0].get());
    CPPUNIT_ASSERT(exps[0]->format() == "Application(FullValue(Variable(hey), Call()), FullValue(Variable(there)))");
  }
  
  void testMapVsIndex() {
    std::vector<PTR<cirth::ast::Expression> > exps;
/*    CPPUNIT_ASSERT(cirth::parser::parse("call. thing [key: value]", exps));
    CPPUNIT_ASSERT(exps.size() == 1);
    CPPUNIT_ASSERT(exps[0].get());
    std::cout << exps[0]->format() << std::endl;
    CPPUNIT_ASSERT(exps[0]->format() == "Application(FullValue(Variable(call), Call()), FullValue(Variable(thing)), FullValue(Map()))");    
    exps.clear();*/
    CPPUNIT_ASSERT(cirth::parser::parse("call. thing[key]", exps));
    CPPUNIT_ASSERT(exps.size() == 1);
    CPPUNIT_ASSERT(exps[0].get());
    CPPUNIT_ASSERT(exps[0]->format() == "Application(FullValue(Variable(call), Call()), FullValue(Variable(thing), Index(Application(FullValue(Variable(key))))))");    
  }
  
  void testCallVsField() {
    std::vector<PTR<cirth::ast::Expression> > exps;
    CPPUNIT_ASSERT(cirth::parser::parse("call. thing1. notafield", exps));
    CPPUNIT_ASSERT(exps.size() == 1);
    CPPUNIT_ASSERT(exps[0].get());
    CPPUNIT_ASSERT(exps[0]->format() == "");    
    exps.clear();
    CPPUNIT_ASSERT(cirth::parser::parse("call. thing1.afield", exps));
    CPPUNIT_ASSERT(exps.size() == 1);
    CPPUNIT_ASSERT(exps[0].get());
    CPPUNIT_ASSERT(exps[0]->format() == "");    
    exps.clear();
    CPPUNIT_ASSERT(cirth::parser::parse("call. function..afield", exps));
    CPPUNIT_ASSERT(exps.size() == 1);
    CPPUNIT_ASSERT(exps[0].get());
    CPPUNIT_ASSERT(exps[0]->format() == "");    
    exps.clear();
    CPPUNIT_ASSERT(cirth::parser::parse("call. function..afuncfield.", exps));
    CPPUNIT_ASSERT(exps.size() == 1);
    CPPUNIT_ASSERT(exps[0].get());
    CPPUNIT_ASSERT(exps[0]->format() == "");    
  }
  
};

CPPUNIT_TEST_SUITE_REGISTRATION(ParserTest);

int main(int argc, char** argv) {
  CPPUNIT_NS::TextUi::TestRunner runner;
  runner.addTest(CPPUNIT_NS::TestFactoryRegistry::getRegistry().makeTest());
  runner.setOutputter(new CPPUNIT_NS::CompilerOutputter(&runner.result(),
      CPPUNIT_NS::stdCOut()));
  return runner.run() ? 0 : 1;
}
