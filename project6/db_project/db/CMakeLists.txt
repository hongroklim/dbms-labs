# Sources
set(DB_SOURCE_DIR src)
set(DB_SOURCES
  # Add your sources here
  ${DB_SOURCE_DIR}/files/file.cc
  ${DB_SOURCE_DIR}/files/FdHolder.cc
  ${DB_SOURCE_DIR}/headers/AHeaderSource.cc
  ${DB_SOURCE_DIR}/headers/FileHeaderSource.cc
  ${DB_SOURCE_DIR}/headers/BufferHeaderSource.cc
  ${DB_SOURCE_DIR}/pages/page.cc
  ${DB_SOURCE_DIR}/pages/AbstractPage.cc
  ${DB_SOURCE_DIR}/pages/HeaderPage.cc
  ${DB_SOURCE_DIR}/indexes/bpt.cc
  ${DB_SOURCE_DIR}/indexes/NodePage.cc
  ${DB_SOURCE_DIR}/indexes/InternalPage.cc
  ${DB_SOURCE_DIR}/indexes/LeafPage.cc
  ${DB_SOURCE_DIR}/buffers/buffer.cc
  ${DB_SOURCE_DIR}/trxes/trx.cc
  ${DB_SOURCE_DIR}/trxes/TrxContainer.cc
  ${DB_SOURCE_DIR}/trxes/LockContainer.cc
  ${DB_SOURCE_DIR}/trxes/LockEntry.cc
  ${DB_SOURCE_DIR}/logs/log.cc
  )

# Headers
set(DB_HEADER_DIR include)
set(DB_HEADERS
  # Add your headers here
  ${DB_HEADER_DIR}/files/file.h
  ${DB_HEADER_DIR}/files/FdHolder.h
  ${DB_HEADER_DIR}/headers/AHeaderSource.h
  ${DB_HEADER_DIR}/headers/FileHeaderSource.h
  ${DB_HEADER_DIR}/headers/BufferHeaderSource.h
  ${DB_HEADER_DIR}/pages/page.h
  ${DB_HEADER_DIR}/pages/AbstractPage.h
  ${DB_HEADER_DIR}/pages/HeaderPage.h
  ${DB_HEADER_DIR}/indexes/bpt.h
  ${DB_HEADER_DIR}/indexes/NodePage.h
  ${DB_HEADER_DIR}/indexes/InternalPage.h
  ${DB_HEADER_DIR}/indexes/LeafPage.h
  ${DB_HEADER_DIR}/buffers/buffer.h
  ${DB_HEADER_DIR}/trxes/trx.h
  ${DB_HEADER_DIR}/trxes/TrxContainer.h
  ${DB_HEADER_DIR}/trxes/LockContainer.h
  ${DB_HEADER_DIR}/trxes/LockEntry.h
  ${DB_HEADER_DIR}/logs/log.h
  )

add_library(db STATIC ${DB_HEADERS} ${DB_SOURCES})

target_include_directories(db
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/${DB_HEADER_DIR}"
  )

