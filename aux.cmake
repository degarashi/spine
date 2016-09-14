# frea: cf93fff9314d046930079249d78519e80d68373b からのコピー
# ファイルリスト[FILELIST]からキーワードリスト[KEYLIST]に該当する物は個別にexecutableを作成、リストから除外
function(MakeSeparationTest KEYLIST FILELIST)
	set(FILELIST_R ${${FILELIST}})
	foreach(KEY IN LISTS KEYLIST)
		ExtractByKeyword(${KEY} FILELIST_R RESULT)
		foreach(SRC IN LISTS RESULT)
			GetFilename(${SRC} FN)
			AddTest(${FN} ${SRC})
		endforeach()
		list(REMOVE_ITEM FILELIST_R ${RESULT})
	endforeach()
	set(${FILELIST} ${FILELIST_R} PARENT_SCOPE)
endfunction()

# パス文字列PTHからファイル名を取り出し、DSTへ格納
function(GetFilename PTH DST)
	string(REGEX MATCH "(.*/)([^\\.]+)\\.(.*)" RES ${PTH})
	if(RES)
		set(${DST} ${CMAKE_MATCH_2} PARENT_SCOPE)
	endif()
endfunction()

# リストSRCのソースファイルを用いてspine_SUBNAME という名前でテストexecutableを作成
function(AddTest SUBNAME SRC)
	list(LENGTH SRC LEN)
	if(${LEN} GREATER 0)
		string(CONCAT EXENAME spine_ ${SUBNAME})
		add_executable(${EXENAME} ${SRC})
		target_link_libraries(${EXENAME}
			${CMAKE_THREAD_LIBS_INIT}
			${GTEST_LIBRARIES}
			${GTEST_MAIN_LIBRARIES}
		)
		add_test(
			NAME ${SUBNAME}
			COMMAND $<TARGET_FILE:${EXENAME}>
		)
	endif()
endfunction()

# リスト[DATA]からKEYを含むソースファイル名をピックアップし[RESULT]へ格納
function(ExtractByKeyword KEY DATA RESULT)
	set(DATA_R ${${DATA}})
	foreach(D IN LISTS DATA_R)
		string(REGEX MATCH "(.*/)*.*${KEY}.*\\.cpp" MR ${D})
		if(MR)
			list(APPEND RES ${D})
		endif()
	endforeach()
	set(${RESULT} ${RES} PARENT_SCOPE)
endfunction()
