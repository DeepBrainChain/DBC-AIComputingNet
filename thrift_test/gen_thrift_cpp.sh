mkdir ./db_thrift_cpp
mkdir ./matrix_thrift_cpp

thrift --gen cpp -out ./db_thrift_cpp ai_db.thrift
thrift --gen cpp -out ./matrix_thrift_cpp matrix.thrift
