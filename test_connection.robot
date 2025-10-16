*** Settings ***
Library    SerialLibrary
Library    String

*** Variables ***
${COM}              COM4
${BAUD}             115200
${BOARD}            nRF5340
${TERMINATOR}       X
${TIMEOUT}          10
${ERROR_LEN}        -1
${ERROR_ARRAY}      -2
${ERROR_VALUE}      -3

*** Test Cases ***
Connect Serial
    Log To Console    Connecting to ${BOARD} on ${COM}
    Add Port    ${COM}    baudrate=${BAUD}    encoding=ascii
    Port Should Be Open    ${COM}
    Reset Input Buffer
    Reset Output Buffer
    Sleep    2s    # Wait for device

Test Valid Time String 000120
    [Documentation]    Test 1 minute 20 seconds = 80 seconds total
    ${result}=    Send Time String And Get Response    000120
    Should Be Equal As Integers    ${result}    80
    Log To Console    Valid time 000120 returned ${result} seconds

Test Valid Time String 000005
    [Documentation]    Test 5 seconds
    ${result}=    Send Time String And Get Response    000005
    Should Be Equal As Integers    ${result}    5
    Log To Console    Valid time 000005 returned ${result} seconds

Test Valid Time String 010203
    [Documentation]    Test 1 hour 2 minutes 3 seconds = 3723 seconds
    ${result}=    Send Time String And Get Response    010203
    Should Be Equal As Integers    ${result}    3723
    Log To Console    Valid time 010203 returned ${result} seconds

Test Invalid Time String Too Short
    [Documentation]    String must be exactly 6 characters
    ${result}=    Send Time String And Get Response    00012
    Should Be Equal As Integers    ${result}    ${ERROR_LEN}
    Log To Console    Short string returned error ${result}

Test Invalid Time String Too Long
    [Documentation]    String must be exactly 6 characters
    ${result}=    Send Time String And Get Response    0001200
    Should Be Equal As Integers    ${result}    ${ERROR_LEN}
    Log To Console    Long string returned error ${result}

Test Invalid Time String With Letters
    [Documentation]    String must contain only digits
    ${result}=    Send Time String And Get Response    00A120
    Should Be Equal As Integers    ${result}    ${ERROR_LEN}
    Log To Console    String with letters returned error ${result}

Test Invalid Time String 67 Seconds
    [Documentation]    Seconds cannot be more than 59
    ${result}=    Send Time String And Get Response    001067
    Should Be Equal As Integers    ${result}    ${ERROR_VALUE}
    Log To Console    67 seconds returned error ${result}

Test Invalid Time String 60 Minutes
    [Documentation]    Minutes cannot be 60 or more
    ${result}=    Send Time String And Get Response    006005
    Should Be Equal As Integers    ${result}    ${ERROR_VALUE}
    Log To Console    60 minutes returned error ${result}

Test Invalid Time String 24 Hours
    [Documentation]    Hours cannot be 24 or more
    ${result}=    Send Time String And Get Response    240000
    Should Be Equal As Integers    ${result}    ${ERROR_VALUE}
    Log To Console    24 hours returned error ${result}

Test Time String Zero Seconds
    [Documentation]    Zero seconds should return 0 (valid but useless for timer)
    ${result}=    Send Time String And Get Response    000000
    Should Be Equal As Integers    ${result}    0
    Log To Console    Zero seconds returned ${result}


Test Valid Sequence RYG
    [Documentation]    Valid 3-character sequence
    ${result}=    Send Sequence And Get Response    RYG
    Should Be Equal As Integers    ${result}    0
    Log To Console    Valid sequence RYG accepted


Disconnect Serial
    Log To Console    Disconnecting from ${BOARD}
    [Teardown]    Delete Port    ${COM}

*** Keywords ***
Send Time String And Get Response
    [Arguments]    ${time_string}
    [Documentation]    Send time string prefixed with T and get parsed result
    Write Data    T${time_string}${TERMINATOR}    encoding=ascii
    Log To Console    Sent: T${time_string}
    ${response}=    Read Until    terminator=88    encoding=ascii    timeout=${TIMEOUT}
    ${response}=    Remove String    ${response}    ${TERMINATOR}
    ${response}=    Strip String    ${response}
    Log To Console    Received: ${response}
    RETURN    ${response}

Send Sequence And Get Response
    [Arguments]    ${sequence}
    [Documentation]    Send LED sequence and get validation result
    Reset Input Buffer
    Write Data    S${sequence}${TERMINATOR}    encoding=ascii
    Log To Console    Sent sequence: ${sequence}
    Sleep    0.1s    # Small delay for device to process
    ${response}=    Read Until    terminator=88    encoding=ascii    timeout=${TIMEOUT}
    ${response}=    Remove String    ${response}    ${TERMINATOR}
    ${response}=    Strip String    ${response}
    Log To Console    Received: ${response}
    RETURN    ${response}

