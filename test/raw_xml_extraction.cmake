execute_process( COMMAND mkdir -p test_output )

execute_process( COMMAND ${TEST_PROG} --cfg ${SOURCEDIR}/rawimg.cfg --overwrite --outdir test_output --imgext pgm --regproc=false --savexml ${SOURCEDIR}/test/test-image.xml
                 RESULT_VARIABLE HAD_ERROR )
if( HAD_ERROR )
    message( FATAL_ERROR "Test failed" )
endif()

execute_process( COMMAND cat test_output/test-image.xml
                 COMMAND diff ${SOURCEDIR}/test/test-image_out.xml -
                 RESULT_VARIABLE DIFFERENT )
if( DIFFERENT )
    message( FATAL_ERROR "Test failed - output xml file differs" )
endif()

execute_process( COMMAND cat test_output/test-image.r1_l1.pgm
                 COMMAND md5sum
                 COMMAND diff ${SOURCEDIR}/test/test-image.r1_l1.pgm.md5 -
                 RESULT_VARIABLE DIFFERENT )
if( DIFFERENT )
    message( FATAL_ERROR "Test failed - output pgm image differs" )
endif()

execute_process( COMMAND rm -r test_output )
