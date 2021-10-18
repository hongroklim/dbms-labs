# Sources
set(DB_SOURCE_DIR src)
set(DB_SOURCES
  ${DB_SOURCE_DIR}/bpt.cc
  ${DB_SOURCE_DIR}/file.cc
  ${DB_SOURCE_DIR}/idx.cc
  ${DB_SOURCE_DIR}/page.cc
    ${DB_SOURCE_DIR}/pages/AbstractPage.cc
    ${DB_SOURCE_DIR}/pages/InternalPage.cc
    ${DB_SOURCE_DIR}/pages/HeaderPage.cc
    ${DB_SOURCE_DIR}/pages/LeafPage.cc
    ${DB_SOURCE_DIR}/pages/NodePage.cc
    ${DB_SOURCE_DIR}/FdHolder.cc
        # Add your sources here
  # ${DB_SOURCE_DIR}/foo/bar/your_source.cc
  )

# Headers
set(DB_HEADER_DIR include)
set(DB_HEADERS
  ${DB_HEADER_DIR}/bpt.h
  ${DB_HEADER_DIR}/file.h
  ${DB_HEADER_DIR}/idx.h
  ${DB_HEADER_DIR}/page.h
    ${DB_HEADER_DIR}/pages/AbstractPage.h
    ${DB_HEADER_DIR}/pages/InternalPage.h
    ${DB_HEADER_DIR}/pages/HeaderPage.h
    ${DB_HEADER_DIR}/pages/LeafPage.h
    ${DB_HEADER_DIR}/pages/NodePage.h
    ${DB_HEADER_DIR}/FdHolder.h
  # Add your headers here
  # ${DB_HEADER_DIR}/foo/bar/your_header.h
  )

add_library(db STATIC ${DB_HEADERS} ${DB_SOURCES})

target_include_directories(db
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/${DB_HEADER_DIR}"
  )

