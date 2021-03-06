/*
 * Copyright (C) 2018 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/**
 * @file iot_demo.c
 * @brief Generic demo runner.
 */

/* The config header is always included first. */
#include "iot_config.h"

/* Standard includes. */
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* SDK initialization include. */
#include "iot_init.h"

/* Error handling include. */
#include "iot_error.h"

/* Common demo includes. */
#include "iot_demo_logging.h"

/* Choose the appropriate network header, initializers, and initialization
 * function. */
#if IOT_NETWORK_USE_OPENSSL == 1
    /* OpenSSL network include. */
    #include "iot_network_openssl.h"

    #define IOT_DEMO_NETWORK_INTERFACE                   IOT_NETWORK_INTERFACE_OPENSSL
    #define IOT_DEMO_SERVER_INFO_INITIALIZER             IOT_NETWORK_SERVER_INFO_OPENSSL_INITIALIZER
    #define IOT_DEMO_CREDENTIALS_INITIALIZER             AWS_IOT_NETWORK_CREDENTIALS_OPENSSL_INITIALIZER
    #define IOT_DEMO_ALPN_FOR_PASSWORD_AUTHENTICATION    AWS_IOT_PASSWORD_ALPN_FOR_OPENSSL

    #define IotDemoNetwork_Init                          IotNetworkOpenssl_Init
    #define IotDemoNetwork_Cleanup                       IotNetworkOpenssl_Cleanup
#else /* if IOT_NETWORK_USE_OPENSSL == 1 */
    /* mbed TLS network include. */
    #include "iot_network_mbedtls.h"

    #define IOT_DEMO_NETWORK_INTERFACE                   IOT_NETWORK_INTERFACE_MBEDTLS
    #define IOT_DEMO_SERVER_INFO_INITIALIZER             IOT_NETWORK_SERVER_INFO_MBEDTLS_INITIALIZER
    #define IOT_DEMO_CREDENTIALS_INITIALIZER             AWS_IOT_NETWORK_CREDENTIALS_MBEDTLS_INITIALIZER
    #define IOT_DEMO_ALPN_FOR_PASSWORD_AUTHENTICATION    AWS_IOT_PASSWORD_ALPN_FOR_MBEDTLS

    #define IotDemoNetwork_Init                          IotNetworkMbedtls_Init
    #define IotDemoNetwork_Cleanup                       IotNetworkMbedtls_Cleanup
#endif /* if IOT_NETWORK_USE_OPENSSL == 1 */

/* This file calls a generic placeholder demo function. The build system selects
 * the actual demo function to run by defining it. */
#ifndef RunDemo
    #error "Demo function undefined."
#endif

/*-----------------------------------------------------------*/

/* Declaration of generic demo function. */
extern int RunDemo( bool awsIotMqttMode,
                    const char * pIdentifier,
                    void * pNetworkServerInfo,
                    void * pNetworkCredentialInfo,
                    const IotNetworkInterface_t * pNetworkInterface );

/*-----------------------------------------------------------*/

int demo( void )
{
    /* Return value of this function and the exit status of this program. */
    IOT_FUNCTION_ENTRY( int, EXIT_SUCCESS );

    /* Status returned from network stack initialization. */
    IotNetworkError_t networkInitStatus = IOT_NETWORK_SUCCESS;

    /* Flags for tracking which cleanup functions must be called. */
    bool sdkInitialized = false, networkInitialized = false;

    /* Network server info and credentials. */
    struct IotNetworkServerInfo serverInfo = IOT_DEMO_SERVER_INFO_INITIALIZER;
    struct IotNetworkCredentials credentials = IOT_DEMO_CREDENTIALS_INITIALIZER,
                                 * pCredentials = NULL;

    /* Set the members of the server info. */
    serverInfo.pHostName = IOT_DEMO_SERVER;
    serverInfo.port = IOT_DEMO_PORT;

    /* For a secured connection, set the members of the credentials. */
    if( IOT_DEMO_SECURED_CONNECTION == true )
    {
        /* Set credential information. */
        credentials.pClientCert = IOT_DEMO_CLIENT_CERT;
        credentials.clientCertSize = strlen( IOT_DEMO_CLIENT_CERT );
        credentials.pPrivateKey = IOT_DEMO_PRIVATE_KEY;
        credentials.privateKeySize = strlen( IOT_DEMO_PRIVATE_KEY );
        credentials.pRootCa = IOT_DEMO_ROOT_CA;
        credentials.rootCaSize = strlen( IOT_DEMO_ROOT_CA );
        credentials.pUserName = NULL;
        credentials.pPassword = NULL;

        /* Set the MQTT username, as long as it's not empty or NULL. */
        if( IOT_DEMO_USER_NAME != NULL )
        {
            credentials.userNameSize = strlen( IOT_DEMO_USER_NAME );

            if( credentials.userNameSize > 0 )
            {
                credentials.pUserName = IOT_DEMO_USER_NAME;
            }
        }

        /* Set the MQTT password, as long as it's not empty or NULL. */
        if( IOT_DEMO_PASSWORD != NULL )
        {
            credentials.passwordSize = strlen( IOT_DEMO_PASSWORD );

            if( credentials.passwordSize > 0 )
            {
                credentials.pPassword = IOT_DEMO_PASSWORD;
            }
        }

        /* By default, the credential initializer enables ALPN with AWS IoT,
         * which only works over port 443. Clear that value if another port is
         * used. */
        if( IOT_DEMO_PORT != 443 )
        {
            credentials.pAlpnProtos = NULL;
        }

        /* Per IANA standard:
         * https://www.iana.org/assignments/tls-extensiontype-values/tls-extensiontype-values.xhtml. */
        if( ( credentials.pUserName != NULL ) &&
            ( IOT_AWS_MQTT_MODE == true ) )
        {
            credentials.pAlpnProtos = IOT_DEMO_ALPN_FOR_PASSWORD_AUTHENTICATION;
        }

        /* Set the pointer to the credentials. */
        pCredentials = &credentials;
    }

    /* Call the SDK initialization function. */
    sdkInitialized = IotSdk_Init();

    if( sdkInitialized == false )
    {
        IOT_SET_AND_GOTO_CLEANUP( EXIT_FAILURE );
    }

    /* Initialize the network stack. */
    networkInitStatus = IotDemoNetwork_Init();

    if( networkInitStatus == IOT_NETWORK_SUCCESS )
    {
        networkInitialized = true;
    }
    else
    {
        IOT_SET_AND_GOTO_CLEANUP( EXIT_FAILURE );
    }

    /* Run the demo. */
    status = RunDemo( IOT_AWS_MQTT_MODE,
                      IOT_DEMO_IDENTIFIER,
                      &serverInfo,
                      pCredentials,
                      IOT_DEMO_NETWORK_INTERFACE );

    IOT_FUNCTION_CLEANUP_BEGIN();

    /* Clean up the network stack if initialized. */
    if( networkInitialized == true )
    {
        IotDemoNetwork_Cleanup();
    }

    /* Clean up the SDK if initialized. */
    if( sdkInitialized == true )
    {
        IotSdk_Cleanup();
    }

    /* Log the demo status. */
    if( status == EXIT_SUCCESS )
    {
        IotLogInfo( "Demo completed successfully." );
    }
    else
    {
        IotLogError( "Error occurred while running demo." );
    }

    IOT_FUNCTION_CLEANUP_END();
}

/*-----------------------------------------------------------*/
