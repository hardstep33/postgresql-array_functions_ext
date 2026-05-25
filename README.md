# postgresql-array_functions_ext
new function for working with array in postgresql 15 (draft)

Как развернуть на любой PostgreSQL 15

Скопировать каталог array_functions_ext/ на сервер, где работает PostgreSQL 15:

Для сборки нужны postgresql-server-dev-15 и make

cd array_functions_ext/

make clean && make && sudo make install

Затем в нужной базе:

CREATE EXTENSION array_functions_ext;
