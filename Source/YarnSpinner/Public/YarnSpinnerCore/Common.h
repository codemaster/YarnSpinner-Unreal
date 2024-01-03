#pragma once

#include <string>
#include <vector>

#define UNUSED(x) (void)(x)

namespace Yarn
{
    struct Line
    {
        FName LineID;
        TArray<FFormatArgumentValue> Substitutions;

        friend std::ostream &operator<<(std::ostream &OutputStream, const Line &AddedLine)
        {
            OutputStream << "ID: " << TCHAR_TO_UTF8(*AddedLine.LineID.ToString());

            int i = 0;
            for (const FFormatArgumentValue& Sub : AddedLine.Substitutions)
            {
                OutputStream << ", {" << i << "} :" << TCHAR_TO_UTF8(*Sub.GetTextValue().ToString());
                i++;
            }
            return OutputStream;
        }
    };

    struct OptionSet
    {
        TArray<struct Option> Options;
    };

    struct Option
    {
        Line Line;
        int ID = -1;
        FString DestinationNode;
        bool IsAvailable = true;
    };

    struct Command
    {
        FString Text;
    };
}