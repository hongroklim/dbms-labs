SELECT T.name
  FROM Trainer T
 WHERE T.hometown = 'Brown city'
    OR T.hometown = 'Rainbow city'
 ORDER BY T.name;
