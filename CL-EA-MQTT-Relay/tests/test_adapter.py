import pytest
import adapter

job_run_id = 1
    #vehicle scripts
script_cids = [
    'bafybeidcuj7x347s2ekyicsu2udaime4dzwf7v5qob446pfspx3j765n7m'
]

def adapter_setup(test_data):
    a = adapter.Adapter(test_data)
    return a.result

#ipfs  data
@pytest.mark.parametrize('vehicle_script_cids', script_cids)
def test_ipfs(vehicle_script_cids):
    test_data = {
        'id': job_run_id, 
        'data': {
            'action': 'ipfs',
            'topic': 'script', 
            'payload': vehicle_script_cids, 
    }}
    result = adapter_setup(test_data)
    # print(result) #Debugging
    assert result['statusCode'] == 200
    assert result['jobRunID'] == job_run_id
    for subtask in result['data']:
        # print(subtask)
        assert type(subtask['payload']['value']) is str
        if test_data['data']['topic'] == 'script':
            assert result['result']['value'] == 'scripted'
        assert subtask['payload']['reporting'] >= 0.5
    assert type(result['result']) is dict