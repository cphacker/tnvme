#ifndef _TEST_H_
#define _TEST_H_

#include <string>
#include <exception>
#include "testDescribe.h"
#include "Singletons/registers.h"

using namespace std;


/**
* This class is the base/interface class for all individual test cases.
* This class purposely enforces children to conform and inherit certain
* functionality to handle mundane tasks.
*
* @note This class will not throw exceptions.
*/
class Test
{
public:
    /**
     * @param fd Pass the opened file descriptor for the device under test
     */
    Test(int fd);
    virtual ~Test();

    /**
     * Get test information
     * @return Test description, short form.
     */
    string GetShortDescription() { return mTestDesc.GetShort(); }

    /**
     * Get test information
     * @param limit80Chars Pass whether to format the long paragraph to 80
     *        character column displays.
     * @param indent Pass num of spaces to indent the long paragraph
     * @return Test description, long form.
     */
    string GetLongDescription(bool limit80Chars, int indent)
        { return mTestDesc.GetLong(limit80Chars, indent); }

    /**
     * Get test information
     * @return Test compliance statement.
     */
    string GetComplianceDescription() { return mTestDesc.GetCompliance(); }

    /**
     * Run the test case. This will catch all exceptions and convert them into
     * a return value, thus children implementing RunCoreTest() are allowed to
     * throw exceptions and they are treated as errors.
     * @return true upon success, otherwise false.
     */
    bool Run();


protected:
    /// file descriptor to the device under test
    int mFd;
    /// Children must populate this during construction
    TestDescribe mTestDesc;

    /**
     * Forcing children to implement the core logic of each test case.
     * @return true upon success, otherwise false. Children are allowed to
     * throw exceptions also, either throwing or the use of return codes is
     * acceptable. Throwing is considered an error.
     */
    virtual bool RunCoreTest() = 0;

    /**
     * Check PCI and ctrl'r registers status registers for errors which may
     * be present.
     */
    bool GetStatusRegErrors();

    /**
     * Report bit position of val which is not like expectedVal
     * @param val Pass value to search against for inequality
     * @param expectedVal Pass the value to compare against for correctness
     * @return INT_MAX if they are equal, otherwise the bit position that isn't
     */
    int ReportOffendingBitPos(ULONGLONG val, ULONGLONG expectedVal);


private:
    Test();
};


#endif
