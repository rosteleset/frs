DELIMITER ;;
CREATE TRIGGER `trg_on_remove_link2` AFTER DELETE ON `link_worker_vstream` FOR EACH ROW 
  if not exists(select * from link_worker_vstream where id_vstream = old.id_vstream) then
    delete from video_streams where id_vstream = old.id_vstream;
  end if ;;
DELIMITER ;
