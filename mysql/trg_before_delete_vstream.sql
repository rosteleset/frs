/* 
Этот триггер появился из-за непофиксенного бага в MySQL: 
Bug #11472	Triggers not executed following foreign key updates/deletes 
Submitted:	21 Jun 2005 0:08
Status:		Verified
*/
CREATE TRIGGER trg_before_delete_vstream
before delete
ON video_streams FOR EACH row
delete from link_descriptor_vstream where id_vstream = old.id_vstream