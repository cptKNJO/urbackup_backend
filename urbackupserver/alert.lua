--find largest (maximum of incr and full interval) interval for file and image backups
local file_interval = params.incr_file_interval
local full_file_interval = params.full_file_interval
if full_file_interval>0 and (full_file_interval<file_interval or file_interval<0)
then
	file_interval = full_file_interval
end

local image_interval = params.incr_image_interval
local full_image_interval = params.full_image_interval
if full_image_interval>0 and (full_image_interval<image_interval or image_interval<0)
then
	image_interval = full_image_interval
end

--ret=0 image and file backup ok
--ret=1 image ok, file backup not ok
--ret=2 image not ok, file backup ok
--ret=3 image and file backup not ok
local ret = 0
--Time in milliseconds to wait till this script is run next
local next_check_ms = 30*60*1000

function pretty_time_append(seconds, name, amount, result)
	if seconds>amount
	then
		local t = math.floor(seconds/amount)
		if result~=""
		then
			result = result .. " "
		end
		result = result .. t .. name
		seconds = seconds - t*amount
	end
	
	return seconds, result
end

--Function to pretty print a duration in seconds
function pretty_time(seconds)
	result = ""
	seconds, result = pretty_time_append(seconds, " month", 30*60*60*24, result)
	seconds, result = pretty_time_append(seconds, " days", 60*60*24, result)
	seconds, result = pretty_time_append(seconds, "h", 60*60, result)
	seconds, result = pretty_time_append(seconds, "min", 60, result)
	if result==""
	then
		result=seconds.. "s"
	end
	return result
end

--Sends an alert mail if mail address was specified as parameter
function fail_mail(image, passed_time, last_time, alert_time)
	if params.alert_nag_interval>0
	then
		if image
		then
			state.last_image_fail_notify = time.unix_seconds()
		else
			state.last_file_fail_notify = time.unix_seconds()
		end
	end

	if params.alert_emails == ""
	then
		return
	end
	
	if alert_time<0
	then
		return
	end

	local btype = "file"
	if image
	then
		btype="image"
		if params.no_images
		then
			return
		end
	elseif params.no_file_backups
	then
		return
	end
	
	local important=" "
	if params.alert_important
	then
		important = "[Important] "
	end
	
	local subj = "[UrBackup]" .. important .. params.clientname .. ": No recent " .. btype .. " backup"
	local lastbackup = "Last " .. btype .." backup was " .. pretty_time(passed_time) .. " ago"
	if last_time==0
	then
		if image
		then
			lastbackup = "Client has never had an image backup"
		else
			lastbackup = "Client has never had a file backup"
		end
	end
	local alert_repeat="."
	if params.alert_nag_interval>0
	then
		alert_repeat = " and then repeated every " .. pretty_time(params.alert_nag_interval/1000) .. " while there is no successfull backup."
	end
	
	local msg = "No recent " .. btype .. " backup for client \"" .. params.clientname .. "\". ".. lastbackup .. ". This alert is sent if there is no recent backup in the last " .. pretty_time(alert_time) .. alert_repeat
	mail(params.alert_emails, subj, msg)
end

--Sends an ok mail if mail address was specified as parameter, and ok mail was enabled
function ok_mail(image, alert_time)
	if params.alert_emails == "" or not params.alert_mail_ok
	then
		return
	end
	
	local btype = "File"
	if image
	then
		btype="Image"
		if params.no_images
		then
			return
		end
	elseif params.no_file_backups
	then
		return
	end
	
	local important=" "
	if params.alert_important
	then
		important = "[Important] "
	end
	
	local subj = "[UrBackup]" .. important .. params.clientname .. ": " .. btype .. " backup status is OK"
	local msg = btype .. " backup status for client \"" .. params.clientname .. "\" is back to ok. Alert was sent because there was no recent backup in the last " .. pretty_time(alert_time) .. "."
	mail(params.alert_emails, subj, msg)
end
	
--Time in seconds till file backup status is not ok
local file_backup_nok = file_interval*params.alert_file_mult - params.passed_time_lastbackup_file
if file_backup_nok<0
then
	ret=ret + 1
	
	--Send warning mail only once on status becoming not ok
	if params.file_ok
	then
		fail_mail(false, params.passed_time_lastbackup_file, params.lastbackup_file, file_interval*params.alert_file_mult )		
	elseif params.alert_nag_interval>0 and (state.last_file_fail_notify==nil or (time.unix_seconds()-state.last_file_fail_notify)*1000>params.alert_nag_interval)
	then
		fail_mail(false, params.passed_time_lastbackup_file, params.lastbackup_file, file_interval*params.alert_file_mult )		
	end

	if params.alert_nag_interval>0
	then
		next_check_ms = math.min(next_check_ms, params.alert_nag_interval)
	end
else
	if not params.file_ok
	then
		ok_mail(false, file_interval*params.alert_file_mult)
	end

	next_check_ms = math.min(next_check_ms, file_backup_nok*1000)
end

if params.image_support
then
	--Time in seconds till image backup status is not ok
	local image_backup_nok = image_interval*params.alert_image_mult - params.passed_time_lastbackup_image
	if image_backup_nok<0
	then
		ret= ret + 2
		
		if params.image_ok
		then
			fail_mail(true, params.passed_time_lastbackup_image, params.lastbackup_image, image_interval*params.alert_image_mult )
		elseif params.alert_nag_interval>0 and (state.last_image_fail_notify==nil or (time.unix_seconds()-state.last_image_fail_notify)*1000>params.alert_nag_interval)
		then
			fail_mail(true, params.passed_time_lastbackup_image, params.lastbackup_image, image_interval*params.alert_image_mult )
		end

		if params.alert_nag_interval>0
		then
			next_check_ms = math.min(next_check_ms, params.alert_nag_interval)
		end
	else
		if not params.image_ok
		then
			ok_mail(true, image_interval*params.alert_image_mult)
		end
		
		next_check_ms = math.min(next_check_ms, image_backup_nok*1000)
	end
else
	ret = ret + 2
end
	
--Second parameter returns the number of milliseconds to wait till the next status check
--If the interval is complex (not a single number, but different numbers depending on window) we cannot return this time reliably
--The alert script is automatically run after a backup so no need to return the wait time if both image and file backup status is not ok
if params.complex_interval
then
	return ret
elseif ret==3
then
	if params.alert_nag_interval>0
	then
		return ret, next_check_ms
	else
		return ret
	end
else
	return ret, next_check_ms
end
