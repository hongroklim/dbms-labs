SELECT T.name
  FROM Trainer T
     , City CT
 WHERE T.hometown = CT.name
   AND CT.name = 'Blue city'
 ORDER BY T.name;
