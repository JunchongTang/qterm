#import <Foundation/Foundation.h>

namespace QTerm {

void disableMacOSPressAndHold()
{
    // Disable the accent picker that intercepts key-hold events on macOS.
    // This only affects the current process and is not written to disk.
    [[NSUserDefaults standardUserDefaults] setBool:NO forKey:@"ApplePressAndHoldEnabled"];
}

} // namespace QTerm
